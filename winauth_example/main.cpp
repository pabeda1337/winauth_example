// winauth_example
//
//   Init() → poll until Connection()==Connected (10s timeout)
//          → prompt creds (user/pass OR license)
//          → Authenticate() — block-poll until callback fires
//          → print product list
//          → loop:  prompt index → LoadProduct() → print result
//          → 'q' to quit
//
// All callbacks fire from winauth::Poll() on this (the only) thread, so a
// `done` bool is enough sync — no atomics, no mutex.

#include "winauth/api.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{
	constexpr int kConnectTimeoutSec = 10;
	constexpr int kFailLingerSec = 5;

	// spin Poll() + sleep until `done` gets set.   
	template <typename Pred>
	void pump_until(Pred&& done)
	{
		while (!done()) {
			winauth::Poll();
			std::this_thread::sleep_for(50ms);
		}
	}

	bool wait_for_connect()
	{
		const auto deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(kConnectTimeoutSec);
		while (std::chrono::steady_clock::now() < deadline) {
			winauth::Poll();
			if (winauth::Connection() == winauth::ConnectionState::Connected) {
				return true;
			}
			std::this_thread::sleep_for(100ms);
		}
		return false;
	}

	std::string prompt_line(const char* msg)
	{
		std::cout << msg << std::flush;
		std::string line;
		if (!std::getline(std::cin, line)) return {};
		return line;
	}

	const char* load_status_label(winauth::LoadStatus s)
	{
		switch (s) {
		case winauth::LoadStatus::Idle:    return "Idle";
		case winauth::LoadStatus::Mapping: return "Mapping";
		case winauth::LoadStatus::Success: return "Success";
		case winauth::LoadStatus::Failed:  return "Failed";
		}
		return "?";
	}

	bool do_authenticate(std::vector<winauth::Product>& products_out)
	{
		std::cout << "\n[1] User & password\n"
			<< "[2] License key\n";
		const std::string choice = prompt_line("Choice [1/2]: ");

		winauth::Credentials creds;
		if (choice == "2") {
			creds.license_key = prompt_line("License key: ");
		}
		else {
			creds.username = prompt_line("Username: ");
			creds.password = prompt_line("Password: ");
		}

		bool done = false;
		winauth::AuthResult            result;
		std::vector<winauth::Product>  prods;

		winauth::Authenticate(creds,
			[&](const winauth::AuthResult& r, const std::vector<winauth::Product>& p) {
				result = r;
				prods = p;
				done = true;
			});

		std::cout << "Authenticating..." << std::endl;
		pump_until([&] { return done; });

		if (result.status == winauth::AuthStatus::Success) {
			std::cout << "OK. Session ID: " << result.session_id;
			if (!result.expires_iso.empty()) {
				std::cout << "  expires " << result.expires_iso;
			}
			std::cout << "\n";
			products_out = std::move(prods);
			return true;
		}

		std::cout << "Authentication failed";
		if (!result.error_code.empty()) {
			std::cout << " [" << result.error_code << "]";
		}
		std::cout << ": " << result.error_message << "\n";
		return false;
	}

	void print_products(const std::vector<winauth::Product>& products)
	{
		std::cout << "\nProducts (" << products.size() << "):\n";
		if (products.empty()) {
			std::cout << "  (no entitled products)\n";
			return;
		}
		for (std::size_t i = 0; i < products.size(); ++i) {
			const auto& p = products[i];
			std::cout << "  [" << i << "] " << p.name
				<< " " << p.version
				<< "  uploaded " << p.uploaded_at
				<< "\n";
		}
	}

	void do_load(const winauth::Product& product)
	{
		bool done = false;
		winauth::LoadResult result;

		winauth::LoadProduct(product.id, [&](const winauth::LoadResult& r) {
			result = r;
			done = true;
			});

		std::cout << "Loading \"" << product.name << "\"..." << std::endl;
		pump_until([&] { return done; });

		std::cout << "Status: " << load_status_label(result.status) << "\n";
		if (result.status == winauth::LoadStatus::Failed) {
			std::cout << "Error:  " << result.error_message << " (code=" << result.error_code << ")\n";
		}
	}

	void load_loop(const std::vector<winauth::Product>& products)
	{
		if (products.empty()) return;

		const std::string line = prompt_line("\nLoad product index ([q] to quit): ");
		if (line == "q" || line == "quit" || line == "exit") return;
		if (line.empty()) return;

		std::size_t idx = 0;
		try {
			idx = std::stoull(line);
		}
		catch (...) {
			std::cout << "  invalid index\n";
			return;
		}
		if (idx >= products.size()) {
			std::cout << "  out of range (have " << products.size() << ")\n";
			return;
		}
		do_load(products[idx]);
	}
}

int main()
{
	std::cout << "winauth host CLI\n"
		<< "----------------\n";

	if (!winauth::Init()) {
		std::cerr << "winauth::Init() failed.\n";
		return 1;
	}

	std::cout << "Connecting to server..." << std::endl;
	if (!wait_for_connect()) {
		std::cerr << "Failed to connect within "
			<< kConnectTimeoutSec << "s.";
		for (int i = kFailLingerSec; i > 0; --i) {
			std::cerr << "  closing in " << i << "s...\n";
			std::this_thread::sleep_for(1s);
		}
		winauth::Shutdown();
		return 1;
	}
	std::cout << "Connected.\n";

	// Retry login until success (or not)
	std::vector<winauth::Product> products;
	while (true) {
		if (do_authenticate(products)) break;
		if (std::cin.eof() || std::cin.fail()) {
			winauth::Shutdown();
			return 1;
		}
	}

	print_products(products);
	load_loop(products);

	std::this_thread::sleep_for(std::chrono::seconds(3));

	winauth::Shutdown();
	return 0;
}
