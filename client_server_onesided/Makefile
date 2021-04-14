
TEST_DIR=tests
CLI_DIR=cli_test

.PHONY: all clean fresh

all:
	$(MAKE) -C $(TEST_DIR)
	$(MAKE) -C $(CLI_DIR)
	
clean:
	$(MAKE) clean -C $(TEST_DIR)
	$(MAKE) clean -C $(CLI_DIR)

fresh:
	$(MAKE) fresh -C $(TEST_DIR)
	$(MAKE) fresh -C $(CLI_DIR)

