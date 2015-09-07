#include "index.h"

#include <utils/exception.h>
#include <utils/utils.h>
#include <boost/iostreams/device/mapped_file.hpp>

using namespace storage;

Index::Index():m_fname("not_set") {
}


Index::~Index() {
}

void Index::setFileName(const std::string& fname) {
	m_fname = fname;
}

std::string Index::fileName()const {
	return m_fname;
}

void Index::writeIndexRec(const Index::IndexRecord &rec) {
	FILE *pFile = std::fopen(this->fileName().c_str(), "ab");

	try {
		fwrite(&rec, sizeof(rec), 1, pFile);
	} catch (std::exception &ex) {
		auto message = ex.what();
		MAKE_EXCEPTION(message);
		fclose(pFile);
	}
	fclose(pFile);
}

std::list<Index::IndexRecord> Index::findInIndex(const IdArray &ids, Time from, Time to) const {
	std::list<Index::IndexRecord> result;

	boost::iostreams::mapped_file i_file;

	try {

		boost::iostreams::mapped_file_params params;
		params.path = this->fileName();
		params.flags = i_file.readwrite;

		i_file.open(params);

		if (!i_file.is_open()) {
			return result;
		}

		IndexRecord *i_data = (IndexRecord *)i_file.data();
		auto fsize = i_file.size();

		bool index_filter = false;
		Id minId = 0;
		Id maxId = 0;
		if (ids.size() != 0) {
			index_filter = true;
			minId = *std::min_element(ids.cbegin(), ids.cend());
			maxId = *std::max_element(ids.cbegin(), ids.cend());
		}

		IndexRecord val;
		val.minTime = from;
		val.maxTime = to;
		IndexRecord *from_pos = std::lower_bound(
			i_data, i_data + fsize / sizeof(IndexRecord), val,
			[](IndexRecord a, IndexRecord b) { return a.minTime < b.minTime; });
		IndexRecord *to_pos = std::lower_bound(
			i_data, i_data + fsize / sizeof(IndexRecord), val,
			[](IndexRecord a, IndexRecord b) { return a.maxTime < b.maxTime; });

		for (auto pos = from_pos; pos <= to_pos; pos++) {
			Index::IndexRecord rec;

			rec = *pos;

			if (utils::inInterval(from, to, rec.minTime) ||
				utils::inInterval(from, to, rec.maxTime)) {
				if (!index_filter) {
					result.push_back(rec);
				} else {
					if (utils::inInterval(minId, maxId, rec.minId) ||
						utils::inInterval(minId, maxId, rec.maxId)) {
						result.push_back(rec);
					}
				}
			}
		}
	} catch (std::exception &ex) {
		auto message = ex.what();
		MAKE_EXCEPTION(message);
		i_file.close();
	}
	i_file.close();

	return result;
}