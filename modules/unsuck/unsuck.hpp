
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <limits>
#include <random>
#include <memory>
#include <algorithm>
#include <thread>
#include <cstdint>
#include <cstring>
#include <regex>

using std::cout;
using std::endl;
using std::to_string;
using std::string;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::fstream;
using std::streamsize;
using std::stringstream;
using std::thread;
using std::ios;
using std::shared_ptr;
using std::make_shared;
using std::chrono::high_resolution_clock;

#ifdef WITH_AWS_SDK
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#endif

namespace fs = std::filesystem;

static long long unsuck_start_time = high_resolution_clock::now().time_since_epoch().count();

static double Infinity = std::numeric_limits<double>::infinity();


#if defined(__linux__)
constexpr auto fseek_64_all_platforms = fseeko64;
#elif defined(WIN32)
constexpr auto fseek_64_all_platforms = _fseeki64;
#endif


struct MemoryData {
	size_t virtual_total = 0;
	size_t virtual_used = 0;
	size_t virtual_usedByProcess = 0;
	size_t virtual_usedByProcess_max = 0;

	size_t physical_total = 0;
	size_t physical_used = 0;
	size_t physical_usedByProcess = 0;
	size_t physical_usedByProcess_max = 0;
};

struct CpuData {
	double usage = 0;
	size_t numProcessors = 0;
};

MemoryData getMemoryData();

CpuData getCpuData();

void printMemoryReport();

void launchMemoryChecker(int64_t maxMB, double checkInterval);

class punct_facet : public std::numpunct<char> {
protected:
	char do_decimal_point() const { return '.'; };
	char do_thousands_sep() const { return '\''; };
	string do_grouping() const { return "\3"; }
};

template<class T>
inline string formatNumber(T number, int decimals = 0) {
	stringstream ss;

	ss.imbue(std::locale(std::cout.getloc(), new punct_facet));
	ss << std::fixed << std::setprecision(decimals);
	ss << number;

	return ss.str();
}

struct Buffer {

	void* data = nullptr;
	uint8_t* data_u8 = nullptr;
	uint16_t* data_u16 = nullptr;
	uint32_t* data_u32 = nullptr;
	uint64_t* data_u64 = nullptr;
	int8_t* data_i8 = nullptr;
	int16_t* data_i16 = nullptr;
	int32_t* data_i32 = nullptr;
	int64_t* data_i64 = nullptr;
	float* data_f32 = nullptr;
	double* data_f64 = nullptr;
	char* data_char = nullptr;

	int64_t size = 0;
	int64_t pos = 0;

	Buffer() {

	}

	Buffer(int64_t size) {
		data = malloc(size);

		if (data == nullptr) {
			auto memory = getMemoryData();

			cout << "ERROR: malloc(" << formatNumber(size) << ") failed." << endl;

			auto virtualAvailable = memory.virtual_total - memory.virtual_used;
			auto physicalAvailable = memory.physical_total - memory.physical_used;
			auto GB = 1024.0 * 1024.0 * 1024.0;

			cout << "virtual memory(total): " << formatNumber(double(memory.virtual_total) / GB) << endl;
			cout << "virtual memory(used): " << formatNumber(double(memory.virtual_used) / GB, 1) << endl;
			cout << "virtual memory(available): " << formatNumber(double(virtualAvailable) / GB, 1) << endl;
			cout << "virtual memory(used by process): " << formatNumber(double(memory.virtual_usedByProcess) / GB, 1) << endl;
			cout << "virtual memory(highest used by process): " << formatNumber(double(memory.virtual_usedByProcess_max) / GB, 1) << endl;

			cout << "physical memory(total): " << formatNumber(double(memory.physical_total) / GB, 1) << endl;
			cout << "physical memory(available): " << formatNumber(double(physicalAvailable) / GB, 1) << endl;
			cout << "physical memory(used): " << formatNumber(double(memory.physical_used) / GB, 1) << endl;
			cout << "physical memory(used by process): " << formatNumber(double(memory.physical_usedByProcess) / GB, 1) << endl;
			cout << "physical memory(highest used by process): " << formatNumber(double(memory.physical_usedByProcess_max) / GB, 1) << endl;

			cout << "also check if there is enough disk space available" << endl;

			exit(4312);
		}

		data_u8 = reinterpret_cast<uint8_t*>(data);
		data_u16 = reinterpret_cast<uint16_t*>(data);
		data_u32 = reinterpret_cast<uint32_t*>(data);
		data_u64 = reinterpret_cast<uint64_t*>(data);
		data_i8 = reinterpret_cast<int8_t*>(data);
		data_i16 = reinterpret_cast<int16_t*>(data);
		data_i32 = reinterpret_cast<int32_t*>(data);
		data_i64 = reinterpret_cast<int64_t*>(data);
		data_f32 = reinterpret_cast<float*>(data);
		data_f64 = reinterpret_cast<double*>(data);
		data_char = reinterpret_cast<char*>(data);

		this->size = size;
	}

	~Buffer() {
		free(data);
	}

	template<class T>
	void set(T value, int64_t position) {
		memcpy(data_u8 + position, &value, sizeof(T));
	}

	inline void write(void* source, int64_t size) {
		memcpy(data_u8 + pos, source, size);

		pos += size;
	}

	template<class T>
	T read(int64_t offset) {
		T value;
		memcpy(&value, this->data_u8 + offset, sizeof(T));

		return value;
	}

};



inline double now() {
	auto now = std::chrono::high_resolution_clock::now();
	long long nanosSinceStart = now.time_since_epoch().count() - unsuck_start_time;

	double secondsSinceStart = double(nanosSinceStart) / 1'000'000'000.0;

	return secondsSinceStart;
}


inline void printElapsedTime(string label, double startTime) {

	double elapsed = now() - startTime;

	string msg = label + ": " + to_string(elapsed) + "s\n";
	cout << msg;
}



inline float random(float min, float max) {

	thread_local std::random_device r;
	thread_local std::default_random_engine e(r());

	std::uniform_real_distribution<float> dist(min, max);

	auto value = dist(e);

	return value;
}

inline std::vector<float> random(float min, float max, int n) {

	thread_local std::random_device r;
	thread_local std::default_random_engine e(r());
	std::uniform_real_distribution<float> dist(min, max);

	std::vector<float> values(n);

	for (int i = 0; i < n; i++) {
		auto value = dist(e);
		values[i] = value;
	}

	return values;
}


inline double random(double min, double max) {

	thread_local std::random_device r;
	thread_local std::default_random_engine e(r());

	std::uniform_real_distribution<double> dist(min, max);

	auto value = dist(e);

	return value;
}

inline std::vector<double> random(double min, double max, int n) {

	thread_local std::random_device r;
	thread_local std::default_random_engine e(r());
	std::uniform_real_distribution<double> dist(min, max);

	std::vector<double> values(n);

	for (int i = 0; i < n; i++) {
		auto value = dist(e);
		values[i] = value;
	}

	return values;
}

inline std::vector<int64_t> random(int64_t min, int64_t max, int64_t n) {

	thread_local std::random_device r;
	thread_local std::default_random_engine e(r());
	std::uniform_int_distribution<int64_t> dist(min, max);

	std::vector<int64_t> values(n);

	for (int i = 0; i < n; i++) {
		auto value = dist(e);
		values[i] = value;
	}

	return values;
}



inline string stringReplace(string str, string search, string replacement) {

	auto index = str.find(search);

	if (index == str.npos) {
		return str;
	}

	string strCopy = str;
	strCopy.replace(index, search.length(), replacement);

	return strCopy;
}

// http://stackoverflow.com/questions/236129/split-a-string-in-c
template<typename Out>
inline void split(const std::string& s, char delim, Out result) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

// http://stackoverflow.com/questions/236129/split-a-string-in-c
inline std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}

// http://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
inline string replaceAll(const std::string& str, const std::string& from, const std::string& to) {
	string tmp = str;

	if (from.empty()) {
		return tmp;
	}

	size_t start_pos = 0;
	while ((start_pos = tmp.find(from, start_pos)) != std::string::npos) {
		tmp.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}

	return tmp;
}

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
inline bool icompare_pred(unsigned char a, unsigned char b) {
	return std::tolower(a) == std::tolower(b);
}

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
inline bool icompare(std::string const& a, std::string const& b) {
	if (a.length() == b.length()) {
		return std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
	} else {
		return false;
	}
}


inline bool endsWith(const string& str, const string& suffix) {

	if (str.size() < suffix.size()) {
		return false;
	}

	auto tstr = str.substr(str.size() - suffix.size());

	return tstr.compare(suffix) == 0;
}

inline bool iEndsWith(const std::string& str, const std::string& suffix) {

	if (str.size() < suffix.size()) {
		return false;
	}

	auto tstr = str.substr(str.size() - suffix.size());

	return icompare(tstr, suffix);
}

#ifdef WITH_AWS_SDK

// Static shared pointer to reuse the S3 client
inline std::shared_ptr<Aws::S3::S3Client> getS3Client() {
    static std::shared_ptr<Aws::S3::S3Client> s3Client;
    static std::once_flag initFlag;

    std::call_once(initFlag, []() {
        auto clientConfig = Aws::Client::ClientConfiguration();
        if (const char* env_p = std::getenv("AWS_ENDPOINT_URL")) {
            clientConfig.endpointOverride = env_p;
        } else if (const char* env_p = std::getenv("AWS_ENDPOINT_URL_S3")) {
            clientConfig.endpointOverride = env_p;
        }

		clientConfig.requestTimeoutMs = 3000;
		clientConfig.connectTimeoutMs = 1000;
		clientConfig.maxConnections = 25; // Increase for higher concurrency
		clientConfig.enableTcpKeepAlive = true;

        bool usePathStyle = false; // Default to virtual-style addressing
        if (const char* env_p = std::getenv("AWS_USE_PATH_STYLE")) {
            std::string env_value(env_p);
            usePathStyle = (env_value == "true" || env_value == "1");
        }

        s3Client = std::make_shared<Aws::S3::S3Client>(
			clientConfig,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
            !usePathStyle
        );
    });

    return s3Client;
}

inline string readAWSS3(string path, string range) {
	auto no_proto = path.substr(5);
	auto parts = split(no_proto, '/');
	auto bucket = parts[0];
	auto key = no_proto.substr(bucket.size() + 1);

	if (std::getenv("DEBUG") == "TRUE") {
		cout << "bucket: " << bucket << endl;
		cout << "key: " << key << endl;
	}

	auto client = getS3Client();
	auto request = Aws::S3::Model::GetObjectRequest();
	request.SetBucket(bucket.c_str());
	request.SetKey(key.c_str());
	if (!range.empty()) {
		if (std::getenv("DEBUG") == "TRUE") {
			cout << "range: " << range << endl;
		}
		request.SetRange(range.c_str());
	}

	auto outcome = client->GetObject(request);

	if (outcome.IsSuccess()) {
		auto& stream = outcome.GetResult().GetBody();
		std::stringstream ss;
		ss << stream.rdbuf();

		return ss.str();
	} else {
		auto error = outcome.GetError();
		std::cerr << "ERROR: " << error.GetExceptionName() << ": " << error.GetMessage() << std::endl;
		exit(1);
	}

	std::stringstream ss;

	return ss.str();

}
#endif

// taken from: https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602060
inline string readTextFile(string path) {

#ifdef WITH_AWS_SDK
	// if path starts with s3://, download the file from s3
	if (path.starts_with("s3://")) {
		return readAWSS3(path, string());
	}
#endif

	std::ifstream t(path);
	std::string str;

	t.seekg(0, std::ios::end);
	str.reserve(t.tellg());
	t.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());

	return str;
}


// taken from: https://stackoverflow.com/questions/18816126/c-read-the-whole-file-in-buffer
// inline vector<char> readBinaryFile(string path) {
// 	std::ifstream file(path, ios::binary | ios::ate);
// 	std::streamsize size = file.tellg();
// 	file.seekg(0, ios::beg);

// 	std::vector<char> buffer(size);
// 	file.read(buffer.data(), size);

// 	return buffer;
// }

inline shared_ptr<Buffer> readBinaryFile(string path) {

#ifdef WITH_AWS_SDK
	// if path starts with s3://, download the file from s3
	if (path.find("s3://") == 0) {
		auto str = readAWSS3(path, string());

		auto buffer = make_shared<Buffer>(str.size());
		memcpy(buffer->data, str.data(), str.size());

		return buffer;
	}
#endif

	auto file = fopen(path.c_str(), "rb");
	auto size = fs::file_size(path);

	//vector<uint8_t> buffer(size);
	auto buffer = make_shared<Buffer>(size);

	fread(buffer->data, 1, size, file);
	fclose(file);

	return buffer;
}

//inline vector<uint8_t> readBinaryFile(string path) {
//
//	auto file = fopen(path.c_str(), "rb");
//	auto size = fs::file_size(path);
//
//	vector<uint8_t> buffer(size);
//
//	fread(buffer.data(), 1, size, file);
//	fclose(file);
//
//	return buffer;
//}

//// taken from: https://stackoverflow.com/questions/18816126/c-read-the-whole-file-in-buffer
//inline vector<uint8_t> readBinaryFile(string path, uint64_t start, uint64_t size) {
//	ifstream file(path, ios::binary);
//	//streamsize size = file.tellg();
//
//	auto totalSize = fs::file_size(path);
//
//	if (start >= totalSize) {
//		return vector<uint8_t>();
//	}if (start + size > totalSize) {
//		auto clampedSize = totalSize - start;
//
//		vector<uint8_t> buffer(clampedSize);
//		file.seekg(start, ios::beg);
//		file.read(reinterpret_cast<char*>(buffer.data()), clampedSize);
//
//		return buffer;
//	} else {
//		vector<uint8_t> buffer(size);
//		file.seekg(start, ios::beg);
//		file.read(reinterpret_cast<char*>(buffer.data()), size);
//
//		return buffer;
//	}
//}

inline vector<uint8_t> readBinaryFile(string path, uint64_t start, uint64_t size) {

#ifdef WITH_AWS_SDK
	// if path starts with s3://, download the file from s3
	if (path.find("s3://") == 0) {
		auto str = readAWSS3(path, "bytes=" + to_string(start) + "-" + to_string(start + size - 1));

		vector<uint8_t> buffer(str.size());
		memcpy(buffer.data(), str.data(), str.size());

		return buffer;
	}
#endif

	//ifstream file(path, ios::binary);

	// the fopen version seems to be quite a bit faster than ifstream
	auto file = fopen(path.c_str(), "rb");

	auto totalSize = fs::file_size(path);

	if (start >= totalSize) {
		return vector<uint8_t>();
	}if (start + size > totalSize) {
		auto clampedSize = totalSize - start;

		vector<uint8_t> buffer(clampedSize);
		//file.seekg(start, ios::beg);
		//file.read(reinterpret_cast<char*>(buffer.data()), clampedSize);
		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(buffer.data(), 1, clampedSize, file);
		fclose(file);

		return buffer;
	} else {
		vector<uint8_t> buffer(size);
		//file.seekg(start, ios::beg);
		//file.read(reinterpret_cast<char*>(buffer.data()), size);
		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(buffer.data(), 1, size, file);
		fclose(file);

		return buffer;
	}
}

inline void readBinaryFile(string path, uint64_t start, uint64_t size, void* target) {

#ifdef WITH_AWS_SDK
	// if path starts with s3://, download the file from s3
	if (path.find("s3://") == 0) {
		auto str = readAWSS3(path, "bytes=" + to_string(start) + "-" + to_string(start + size - 1));

		memcpy(target, str.data(), str.size());

		return;
	}
#endif

	auto file = fopen(path.c_str(), "rb");

	auto totalSize = fs::file_size(path);

	if (start >= totalSize) {
		return;
	}if (start + size > totalSize) {
		auto clampedSize = totalSize - start;

		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(target, 1, clampedSize, file);
		fclose(file);
	} else {
		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(target, 1, size, file);
		fclose(file);
	}
}

// writing smaller batches of 1-4MB seems to be faster sometimes?!?
// it's not very significant, though. ~0.94s instead of 0.96s.
template<typename T>
inline void writeBinaryFile(string path, vector<T>& data) {
	std::ios_base::sync_with_stdio(false);
	auto of = fstream(path, ios::out | ios::binary);

	int64_t remaining = data.size() * sizeof(T);
	int64_t offset = 0;

	while (remaining > 0) {
		constexpr int64_t mb4 = int64_t(4 * 1024 * 1024);
		int batchSize = std::min(remaining, mb4);
		of.write(reinterpret_cast<char*>(data.data()) + offset, batchSize);

		offset += batchSize;
		remaining -= batchSize;
	}


	of.close();
}

inline void writeBinaryFile(string path, Buffer& data) {
	std::ios_base::sync_with_stdio(false);
	auto of = fstream(path, ios::out | ios::binary);

	int64_t remaining = data.size;
	int64_t offset = 0;

	while (remaining > 0) {
		constexpr int64_t mb4 = int64_t(4 * 1024 * 1024);
		int batchSize = std::min(remaining, mb4);
		of.write(reinterpret_cast<char*>(data.data) + offset, batchSize);

		offset += batchSize;
		remaining -= batchSize;
	}


	of.close();
}

// taken from: https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602060
inline string readFile(string path) {

	std::ifstream t(path);
	std::string str;

	t.seekg(0, std::ios::end);
	str.reserve(t.tellg());
	t.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());

	return str;
}

inline void writeFile(string path, string text) {

	ofstream out;
	out.open(path);

	out << text;

	out.close();
}



inline void logDebug(string message) {
#if defined(_DEBUG)

	auto id = std::this_thread::get_id();

	stringstream ss;
	ss << "[" << id << "]: " << message << "\n";

	cout << ss.str();
#endif
}



template<typename T>
T read(vector<uint8_t>& buffer, int offset) {
	//T value = reinterpret_cast<T*>(buffer.data() + offset)[0];
	T value;

	memcpy(&value, buffer.data() + offset, sizeof(T));

	return value;
}



inline string leftPad(string in, int length, const char character = ' ') {

	int tmp = length - in.size();
	auto reps = std::max(tmp, 0);
	string result = string(reps, character) + in;

	return result;
}

inline string rightPad(string in, int64_t length, const char character = ' ') {

	auto reps = std::max(length - int64_t(in.size()), int64_t(0));
	string result = in + string(reps, character);

	return result;
}



inline vector<string> getRegexMatches(string str, string strPattern) {

	vector<string> matches;

	std::regex pattern(strPattern, std::regex_constants::ECMAScript | std::regex_constants::icase);

	auto it = std::sregex_iterator(str.begin(), str.end(), pattern);
	auto end = std::sregex_iterator();
	for (; it != end; it++) {
		std::smatch match = *it;
		std::string match_str = match.str();

		matches.push_back(match_str);
	}

	return matches;
}


#define GENERATE_ERROR_MESSAGE cout << "ERROR(" << __FILE__ << ":" << __LINE__ << "): "
#define GENERATE_WARN_MESSAGE cout << "WARNING: "



