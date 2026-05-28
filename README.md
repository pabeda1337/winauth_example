# winauth_sample, provided by https://winauth.net/
This project serves as an example product loader for the customers of WinAuth.

This loader was tested built via MSVC and is ready to be built and used, under certain circumstanes.

**Requirements:**
- Have an active winauth subscription (Starter/Pro/Enterprise)
- Have your product uploaded to the [WinAuth dashboard](https://winauth.net/dashboard).
- Windows 10/11

**Setup:**
- Clone the repository
- Navigate to [WinAuth dashboard](https://winauth.net/dashboard) and download your customer stub
- Put the downloaded _winauth.dll_ into the _winauth_example/winauth_ folder
> Keep in mind, the placement of winauth.dll is crucial as the Pre/Post build events rely on it and so does the whole functionality of the loader
- Open the project in Visual Studio and Build the project
- Open the output.exe and you're done!

Your application is now ready to be shipped. You are free to modify the client loader source.
We recommend utilizing [our Public API](https://winauth.net/api/docs#description/introduction) in your products.

**For Enterprise clients:**
People with the Enterprise package are highly recommended to use our Tiger obfuscator, which is available at [the Tiger dashboard](https://winauth.net/dashboard/obfuscation).
