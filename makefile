all: HttpWebProxy

# Tool invocations
HttpWebProxy:
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc src/ProxyServer.c -o webproxy -lssl -lcrypto
	@echo 'Finished building target: $@'

