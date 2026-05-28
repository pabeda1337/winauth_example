#pragma once

// Public API for the winauth DLL. Designed to be consumed by host applications
// that link against winauth.lib + winauth.dll.
//
// IMPORTANT ABI NOTES
// -------------------
// This API uses C++ types (std::string, std::vector, std::function) across the
// DLL boundary. That keeps the surface ergonomic but is only safe when the
// DLL and host are built with **the same MSVC toolset version, the same C++
// standard, and the same C runtime linkage (/MD or /MT)**. Mixing those
// produces silent ABI breakage — strings/vectors will appear corrupt or leak.
//
// THREADING
// ---------
// All callbacks passed to the API are invoked from `winauth::Poll()` on the
// thread that calls Poll() — typically the UI/render thread of the host. The
// internal IO thread never calls into user code directly. Host applications
// must call `Poll()` every frame (or at least often enough that pending
// callbacks don't pile up).
//
// All other API entry points are thread-safe and may be called from any
// thread.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#define WINAUTH_API __declspec(dllimport)

namespace winauth
{
	// ---- Init / lifecycle -----------------------------------------------------

	// Spins up the IO thread, and starts the TLS connection in the
	// background. Successful return does NOT mean
	// we've connected — watch `Connection()` for that.
	WINAUTH_API bool Init();

	// Stops the IO thread, closes any connection, releases all
	// resources. Safe to call from the same thread that called Init().
	WINAUTH_API void Shutdown();

	// Pumps pending callbacks and state updates onto the calling thread.
	// Should be called every frame from the host's UI thread.
	WINAUTH_API void Poll();

	// ---- Connection state -----------------------------------------------------

	enum class ConnectionState
	{
		Disconnected,
		Connecting,
		Connected,
		Reconnecting,
	};

	WINAUTH_API ConnectionState Connection();

	// ---- Authentication -------------------------------------------------------

	// Username/password OR license key; if `license_key` is set the other two
	// are ignored.
	struct Credentials
	{
		std::string username;
		std::string password;
		std::string license_key;
	};

	enum class AuthStatus
	{
		Idle,
		InProgress,
		Success,
		Failed,
	};

	// One product the authenticated user is entitled to load.
	struct Product
	{
		std::uint64_t id            = 0;
		std::string   name;
		std::string   version;
		std::string   sha256;
		std::uint32_t size_of_image = 0;
		std::uint64_t file_size     = 0;
		std::int64_t  updated_at    = 0;    // unix epoch, 0 if not parsed
		std::string   uploaded_at;            // raw ISO-8601 from backend
	};

	struct AuthResult
	{
		AuthStatus    status         = AuthStatus::Idle;
		std::uint64_t session_id     = 0;
		std::int64_t  expires_at     = 0;    // unix epoch, 0 if backend didn't supply
		std::string   expires_iso;            // raw ISO-8601 from backend, empty otherwise
		std::string   error_code;             // e.g. "INVALID_LICENSE", "HWID_MISMATCH"
		std::string   error_message;
	};

	// Callback signature for `Authenticate`. Always invoked exactly once from
	// `Poll()` after the request completes. On success the `products` vector
	// contains the catalogue the host can pass to `LoadProduct`.
	using AuthCallback =
		std::function<void(const AuthResult&, const std::vector<Product>&)>;

	WINAUTH_API void Authenticate(const Credentials& creds, AuthCallback on_complete);

	// Snapshot of the last completed authentication. AuthStatus::Idle until
	// the first call returns.
	WINAUTH_API const AuthResult& Auth();

	// Returns the product catalogue from the most recent successful auth.
	// Empty before / between successful auths. Safe to call from any thread.
	WINAUTH_API const std::vector<Product>& Products();

	// ---- Load product ---------------------------------------------------------

	enum class LoadStatus
	{
		Idle,
		Mapping,
		Success,
		Failed,
	};

	struct LoadResult
	{
		LoadStatus     status        = LoadStatus::Idle;
		std::uint64_t  build_id      = 0;
		bool           mapped_remote = false;
		std::uint64_t  base_address  = 0;
		std::uint32_t  image_size    = 0;
		std::uint32_t  error_code    = 0;
		std::string    error_message;
	};

	using LoadCallback = std::function<void(const LoadResult&)>;

	// Kicks off the two-step manual-map flow for `build_id`. Status is
	// observable via `Load()` between callbacks. Calling while another load
	// is in flight returns immediately with `error_code` set to "busy".
	WINAUTH_API void LoadProduct(std::uint64_t build_id, LoadCallback on_complete);

	// Current load progress; updated by Poll().
	WINAUTH_API const LoadResult& Load();

} // namespace winauth
