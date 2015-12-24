#include <ctime>
#include <iostream>
#include <cstdlib>

#include <nkvdb.h>
#include <logger.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

const std::string storage_path = "benchPage.page";

size_t meas2write = 10;
bool tp_bench = false;
bool write_only = false;

void makeAndWrite(int mc, int psize) {
  logger("makeAndWrite mc:" << mc<<" psize:"<<psize);

  const uint64_t storage_size = nkvdb::Page::calc_size(psize);
  auto page_path = storage_path;
  nkvdb::Page::Page_ptr ds =
      nkvdb::Page::Create(page_path, storage_size);

  clock_t write_t0 = clock();
  nkvdb::Meas meas = nkvdb::Meas::empty();
  nkvdb::Meas::PMeas pm = new nkvdb::Meas[psize];
  for (int i = 0; i < psize; ++i) {
    meas.setValue(i%mc);
    meas.id = i % mc;
    meas.source = meas.flag = i % mc;
    meas.time = time(0);

	pm[i] = meas;
  }
  auto ares=ds->append(pm, psize);
  clock_t write_t1 = clock();
  logger("write time: " << ((float)write_t1 - write_t0) / CLOCKS_PER_SEC);
  delete[] pm;
  ds->close();
  ds = nullptr;
  
  nkvdb::utils::rm(page_path);
  nkvdb::utils::rm(page_path + "i");
  nkvdb::utils::rm(page_path + "w");
}

void readIntervalBench(nkvdb::Page::Page_ptr ds,   nkvdb::Time from, nkvdb::Time to,   std::string message) {

  clock_t read_t0 = clock();
  nkvdb::Meas::MeasList meases {};
  auto reader = ds->readInterval(from, to);
  reader->readAll(&meases);
  clock_t read_t1 = clock();

  logger("=> : " << message << " time: " << ((float)read_t1 - read_t0) / CLOCKS_PER_SEC);
}

void readIntervalBenchFltr(nkvdb::IdArray ids, nkvdb::Flag src,
                           nkvdb::Flag flag,
						   nkvdb::Page::Page_ptr ds,
                           nkvdb::Time from, nkvdb::Time to,
                           std::string message) {
  clock_t read_t0 = clock();
  
  nkvdb::Meas::MeasList output;
  auto reader = ds->readInterval(ids, src, flag, from, to);
  reader->readAll(&output);
  clock_t read_t1 = clock();

  logger("=> :" << message << " time: " << ((float)read_t1 - read_t0) / CLOCKS_PER_SEC);
}

void timePointBench(nkvdb::IdArray ids, nkvdb::Flag src,
						   nkvdb::Flag flag,
						   nkvdb::Page::Page_ptr ds,
						   nkvdb::Time from,
						   std::string message, size_t count) {
	clock_t read_t0 = clock();

	for (size_t i = 0; i < count; i++) {
		nkvdb::Meas::MeasList output;
		auto reader = ds->readInTimePoint(ids, src, flag, from);
		reader->readAll(&output);
	}
	clock_t read_t1 = clock();
    logger("=> :" << message << " time: " << ((float)read_t1 - read_t0) / CLOCKS_PER_SEC/count);
}


int main(int argc, char *argv[]) {
  po::options_description desc("IO benchmark.\n Allowed options");
  desc.add_options()("help", "produce help message")(
      "mc", po::value<size_t>(&meas2write)->default_value(meas2write), "measurment count")
	  ("tpbench", po::value<bool>(&tp_bench)->default_value(tp_bench), "enable time point benchmark")
      ("write-only", "don`t run readInterval")
      ("verbose", "verbose ouput")
      ("dont-remove", "dont remove created storage");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
  } catch (std::exception &ex) {
    logger("Error: " << ex.what());
    exit(1);
  }
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  if (vm.count("write-only")) {
    write_only = true;
  }

  makeAndWrite(meas2write, 1000000);
  makeAndWrite(meas2write, 2000000);
  makeAndWrite(meas2write, 3000000);

  if (!write_only) {
	  int pagesize = 1000000;
      const uint64_t storage_size = nkvdb::Page::calc_size(pagesize);

    nkvdb::Page::Page_ptr ds = nkvdb::Page::Create(storage_path, storage_size);
    nkvdb::Meas meas = nkvdb::Meas::empty();

    logger( "creating storage...");
    logger( "pages_size:" << pagesize);

	nkvdb::Meas::PMeas pm = new nkvdb::Meas[pagesize];
	for (int64_t i = 0; i < pagesize; ++i) {
      meas.setValue(i);
      meas.id = i % meas2write;
      meas.source = meas.flag = i % meas2write;
      meas.time = i;
	  pm[i] = meas;
    }
	ds->append(pm, pagesize);

	logger("big readers");
	readIntervalBench(ds, 0, pagesize / 2, "[0-0.5]");
	readIntervalBench(ds, pagesize / 2, pagesize, "[0.5-1.0]");
	readIntervalBench(ds, pagesize*0.7, pagesize * 0.95, "[0.7-0.95]");

	logger("small readers");
	readIntervalBench(ds, pagesize * 0.3, pagesize*0.6, "[0.3-0.6]");
	readIntervalBench(ds, pagesize*0.6, pagesize * 0.9, "[0.6-0.9]");
	readIntervalBench(ds, pagesize * 0.3, pagesize * 0.7, "[0.3-0.7]");

    logger("fltr big readers");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 2, 3, 4, 5}, 1, 1, ds, 0,
                          pagesize / 2, "Id: {0- 5}, src:1, flag:1; [0-0.5]");

    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 1, 0,
                          ds, pagesize / 2, pagesize * 0.9,
                          "Id: {0- 9}, src:1, flag:0; [0.5-0.9]");

    readIntervalBenchFltr(
        nkvdb::IdArray{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 1, 1, ds,
        7 * pagesize, 8 * pagesize + pagesize * 1.5,
        "Id: {0-12}, src:1, flag:1; [7-9.5]");

    logger("fltr small readers");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1}, 1, 1, ds,
                          pagesize * 0.3, pagesize*0.6,
                          "Id: {0,1},   src:1,  flag:1; [0.3-0.6]");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 3}, 1, 1, ds, pagesize*0.2,
                          pagesize * 0.5,
                          "Id: {0,1,3}, src:1,  flag:1; [0.2-0.5]");
    readIntervalBenchFltr(nkvdb::IdArray{0}, 1, 1, ds, pagesize * 0.3,
                          pagesize * 0.7,
                          "Id: {0},     src:1,  flag:1; [0.3-0.7]");
    
	if (tp_bench) {
		logger("timePoint fltr big readers");
		timePointBench(nkvdb::IdArray{ 0, 1, 2, 3, 4, 5 }, 1, 1, ds, 0,
					   "Id: {0- 5}, src:1, flag:1; [0]", 5);

		timePointBench(nkvdb::IdArray{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }, 1, 0,
					   ds, pagesize / 2,
					   "Id: {0- 9}, src:1, flag:0; [0.5]", 5);

		timePointBench(
			nkvdb::IdArray{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, 1, 1, ds,
			pagesize * 0.95,
			"Id: {0-12}, src:1, flag:1; [0.95]", 5);

		logger("timePoint fltr small readers");
		timePointBench(nkvdb::IdArray{ 0, 1 }, 1, 1, ds,
					   pagesize / 3,
					   "Id: {0,1},   src:1,  flag:1; [0.3]", 5);
		timePointBench(nkvdb::IdArray{ 0, 1, 3 }, 1, 1, ds,
					   pagesize * 0.5,
					   "Id: {0,1,3}, src:1,  flag:1; [0.5]", 5);
		timePointBench(nkvdb::IdArray{ 0 }, 1, 1, ds, pagesize * 0.6,
					   "Id: {0},     src:1,  flag:1; [0.6]", 5);
	}
	ds->close();
    ds=nullptr;

    nkvdb::utils::rm(storage_path);
	nkvdb::utils::rm(storage_path+"i");
	nkvdb::utils::rm(storage_path + "w");
  }
}
