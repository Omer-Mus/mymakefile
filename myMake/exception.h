#pragma once
#include <stdexcept>
#include <string>

/*
 * Part 2a: Defining custom exceptions
 */

namespace mymake {
namespace exception {

class Fatal : public std::runtime_error {
	using std::runtime_error::runtime_error;
};
class NonFatal : public std::runtime_error {
	using std::runtime_error::runtime_error;
};

class ParseError : public Fatal {
public:
	ParseError(int lineno, const std::string& reason)
		: Fatal("MyMakefile:" + std::to_string(lineno) + ": *** " + reason + ".  Stop.")
	{}
};

class NoMyMakefile : public Fatal {
public:
	NoMyMakefile()
		: Fatal("mymake: *** No mymakefile found.  Stop.")
	{}
};

class NoTargets : public Fatal {
public:
	NoTargets()
		: Fatal("mymake: *** No targets.  Stop.")
	{}
};

class TargetNoRule : public Fatal {
public:
	TargetNoRule(const std::string& target)
		: Fatal("mymake: *** No rule to make target '" + target + "'.  Stop.")
	{}
};

class TargetCommandFailed : public Fatal {
private:
	const int exit_status;
public:
	TargetCommandFailed(const std::string& str, int e)
		: Fatal("mymake: *** [" + str + "] Error " + std::to_string(e)),
		  e_status(e)
	{}

	int exit_status() const noexcept { return exit_status; }


};

class TargetNothingToDo : public NonFatal {
public:
	TargetNothingToDo(const std::string& tar)
		: NonFatal("mymake: Nothing to be done for '" + tar + "'.")
	{}
};

class TargetUpToDate : public NonFatal {
public:
	TargetUpToDate(const std::string& tar)
		: NonFatal("mymake: '" + tar + "' is up to date.")
	{}
};

} // namespace exception
} // namespace mymake
