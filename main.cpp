#include <iostream>
#include <memory>
#include <stdexcept>

class FilePtr {
	std::FILE* f_ = nullptr;

public:
	explicit FilePtr(const char* path, const char* mode)
		: f_(std::fopen(path, mode)) {}

	// Destructor (unchanged)
	~FilePtr() {
		if (f_) std::fclose(f_);
	}

	// Move constructor
	FilePtr(FilePtr&& other) noexcept : f_(other.f_) {
		other.f_ = nullptr;                     // source becomes empty
	}

	// Move assignment
	FilePtr& operator=(FilePtr&& other) noexcept {
		if (this != &other) {
			if (f_) std::fclose(f_);           // release current resource
			f_ = other.f_;                     // steal
			other.f_ = nullptr;                // empty the source
		}
		return *this;
	}

	// Copy operations remain deleted
	FilePtr(const FilePtr&) = delete;
	FilePtr& operator=(const FilePtr&) = delete;

	std::FILE* get() const { return f_; }
};

class Token {
	bool valid_;

public:
	Token(bool valid) : valid_(valid) {
		if (!valid_) {
			throw std::runtime_error("Exception Triggered");
		}
	}

	~Token() = default;
};

struct Session {
	FilePtr log_;
	std::unique_ptr<int[]> buf_;
	Token auth_;

public:
	Session(std::size_t n, const char* logfile, bool valid) :
		log_(logfile,"w"), buf_(std::make_unique<int[]>(n)), auth_(valid) {}

	std::FILE* log() const {return log_.get();}
};

void write_log(Session& s, const char* msg) {
	std::fputs(msg, s.log_.get());
}

int main()
{
	try {
		Session s(10,"file.txt",true);
		write_log(s, "Hello\n");
	} catch (std::exception& e) {
		std::cout << e.what() << '\n';
	}

	return 0;
}