SHELL := /bin/bash
TEST_FILES := $(shell find src -name '*.ts')
BIN := ./node_modules/.bin

build: | build/dir
	cdt-cpp -abigen -abigen_output=build/epoch.drops.abi -o build/epoch.drops.wasm src/epoch.drops.cpp -R src -I include -D DEBUG

build/dir:
	mkdir -p build

clean:
	rm -rf build

.PHONY: test
test: build node_modules build/epoch.drops.ts init/codegen
	bun test

init/codegen: codegen/dir codegen/eosio.ts codegen/eosio.token.ts 

build/epoch.drops.ts: 
	npx @wharfkit/cli generate --json ./build/epoch.drops.abi --file ./build/epoch.drops.ts drops

codegen: codegen/clean codegen/dir codegen/drops.ts codegen/eosio.ts codegen/eosio.token.ts

codegen/clean:
	rm -rf codegen

codegen/dir:
	mkdir -p codegen

codegen/drops.ts:
	npx @wharfkit/cli generate --json ../drops/build/drops.abi --file ./codegen/drops.ts drops

codegen/eosio.ts:
	npx @wharfkit/cli generate --url https://jungle4.greymass.com --file ./codegen/eosio.ts eosio

codegen/eosio.token.ts:
	npx @wharfkit/cli generate --url https://jungle4.greymass.com --file ./codegen/eosio.token.ts eosio.token

.PHONY: check
check: cppcheck jscheck

.PHONY: cppcheck
cppcheck: 
	clang-format --dry-run --Werror src/*.cpp include/drops/*.hpp

.PHONY: jscheck
jscheck: node_modules
	@${BIN}/eslint src --ext .ts --max-warnings 0 --format unix && echo "Ok"

.PHONY: format
format: cppformat jsformat

.PHONY: cppformat
cppformat:
	clang-format -i src/*.cpp include/drops/*.hpp

.PHONY: jsformat
jsformat: node_modules
	@${BIN}/eslint src --ext .ts --fix

.PHONY: distclean
distclean: clean
	rm -rf node_modules/

node_modules:
	bun install
