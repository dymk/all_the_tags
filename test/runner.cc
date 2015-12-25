#include <hayai.hpp>
#include "gtest/gtest.h"

#include "context.h"

int main(int argc, char** argv) {
  bool bench_mem = false;
  if(argc == 2 && strcmp(argv[1], "--bench-mem") == 0) {
    bench_mem = true;
  }

  if(bench_mem) {
      Context c;

      for(int i = 0; i < 1000000; i++) { c.new_entity(i); }
      for(int i = 0; i < 1500000; i++) { c.new_tag(i); }

      std::cout << "created 1M entities, 1M tags...";
  }
  else {
      // run google tests!
      ::testing::InitGoogleTest(&argc, argv);
      if(RUN_ALL_TESTS() != 0) { return -1; }

      hayai::ConsoleOutputter consoleOutputter;
      hayai::Benchmarker::AddOutputter(consoleOutputter);
      hayai::Benchmarker::RunAllTests();
  }

  return 0;
}
