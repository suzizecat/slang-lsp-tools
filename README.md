## Build instruction

Given that you have a modern C++ compiler (able to handle C++20), do:
```shell
git clone git@github.com:suzizecat/slang-lsp-tools.git
cd slang-lsp-tools
cmake -DCMAKE_BUILD_TYPE=Release -B./build
cmake --build ./build --target slang-lsp -j `nproc`
```

And the LSP Server should be built in `./build/slang-lsp`.

If you have the  `mold` linked, you can use to use it.
```shell
cmake -DCMAKE_BUILD_TYPE=Release -DDIPLOMAT_USE_MOLD=ON -B./build
```

## Usage
To run the server linked to the vscode extension, start the server with 
```bash
./build/slang-lsp --tcp
```

and starts the client once you get the message.
```
[info] Await client on port 8080...
```



