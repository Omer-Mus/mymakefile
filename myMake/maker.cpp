#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <cstdlib>
#include <sys/wait.h>
#include <xdb.h>
#include "exception.h"
#include "helper.h"
#include "maker.h"

using namespace mymake;
namespace fs = std::filesystem;

Maker::Maker()
{


	static constexpr const char *MYMAKEFILE = "MyMakefile";
	static constexpr const char *CACHEFILE  = ".mymake.cache";

	// Use cached rules if present and newer
	if (fs::exists(MYMAKEFILE) && fs::exists(CACHEFILE) && 
	    fs::last_write_time(MYMAKEFILE) <= fs::last_write_time(CACHEFILE)) {

		// Open cache file for reading
		xdb::XdbReader<std::pair<std::string, Rule>> cache(CACHEFILE);

		// Iterate over cache
		for (size_t i = 0; i < cache.size(); ++i) {
			const Rule& rule = cache[i].second;

			// Check if implicit rule need to be re-generated
			if (!rule.phony) {
				if (rule.commands.empty()) {
					std::string stem(fs::path(cache[i].first).stem());
					if (fs::exists(stem + ".cpp") ||
					    fs::exists(stem + ".c"))
						break;
				}
				else if (rule.implicit_dep && rule.deps().size()) {
					const std::string& dep = rule.deps().front();
					std::string ext(fs::path(dep).extension());
					if (ext == ".cpp" || ext == ".c")
						if (!fs::exists(dep))
							break;
				}
			}

			// Insert rule from cache; default target is the first one
			auto ins = rules.insert(cache[i]);
			if (i == 0)
				default_rule = &*ins.first;
		}

		// If all rules good, then we're done
		if (rules.size() == cache.size() && default_rule)
			return;

		// Otherwise clear rules and proceed to normal logic
		rules.clear();
		default_rule = nullptr;

	}

	/*
	 * Part 2b: Parsing the MyMakefile
	 */

	static const std::regex comm_ln("^\\t(.*)$");
	static const std::regex assign_ln("^(.*?)=(.*)$");
	static const std::regex rule_ln("^(.*?):(.*)$");
	static const std::regex var("\\$\\((.*?)\\)");

	std::unordered_map<std::string, std::string> vars = {
		{ "CC", "cc" },
		{ "CXX", "c++" },
	};

	// Open MyMakefile; throw appropriate exception on failure
	std::ifstream mf(MYMAKEFILE);
	if (mf.fail()) {
		if (errno == ENOENT)
			throw exception::NoMyMakefile();
		else
			throw std::system_error(errno, std::generic_category(), "std::ifstream");
	}

	// Read through MyMakefile
	std::vector<std::string> cur_targets;
	std::smatch sm;
	std::string ln, rest;
	for (int lineno = 1; std::getline(mf, ln); ++lineno) {

		// Truncate at '#' comments
		size_t comm_pos = ln.find_first_of('#');
		if (comm_pos != std::string::npos)
			ln = ln.substr(0, comm_pos);

		// Skip empty lines
		if (helper::trim(ln).empty())
			continue;

		// Replace variable occurrences
		rest = ln;
		while (std::regex_search(rest, sm, var)) {
			ln.replace(ln.find(sm[0]), sm.length(0), vars[helper::trim(sm[1])]);
			rest = sm.suffix();
		}

		// Command line
		if (std::regex_match(ln, sm, comm_ln)) {
			if (cur_targets.empty())
				throw exception::ParseError(lineno,
				                            "commands commence before first target");
			for (const std::string& target : cur_targets)
				rules.at(target).commands.push_back(sm[1]);
		}
		// Variable assign line
		else if (std::regex_match(ln, sm, assign_ln)) {
			std::string var(helper::trim(sm[1]));
			if (var.empty())
				throw exception::ParseError(lineno, "empty variable name");
			vars[std::move(var)] = helper::trim(sm[2]);
		}
		// Rule line
		else if (std::regex_match(ln, sm, rule_ln)) {
			cur_targets = helper::split(sm[1]);
			std::vector<std::string> deps(helper::split(sm[2]));
			for (const std::string& target : cur_targets) {
				Rule& rule = rules[target];
				bool is_phony = target == ".PHONY";
				if (is_phony)
					rule.phony = is_phony;
				for (const std::string& dep : deps) {
					rule.push_dep(dep);
					if (is_phony)
						rules[dep].phony = true;
				}
				if (!default_rule && !is_phony)
					default_rule = &*rules.find(target);
			}
		} else
			throw exception::ParseError(lineno, "missing separator");

	}
	if (mf.bad())
		throw std::system_error(errno, std::generic_category(), "std::getline");

	// No target parsed; throw exception
	if (!default_rule)
		throw exception::NoTargets();

	/*
	 * Part 2c: Implicit rule generation
	 */

	// Go over parsed and generate implicit rules as needed
	for (auto& [target, rule] : rules) {

		// Only do implicit rules if no commands and isn't phony
		if (!rule.commands.empty() || rule.phony)
			continue;

		fs::path fname(target);
		std::string dep, comm;

		// Set implicit command and dependency for object file target
		if (fname.extension() == ".o") {
			std::string stem(fname.stem());
			if (fs::exists(dep = stem + ".cpp"))
				comm = vars["CXX"] + " " + vars["CXXFLAGS"];
			else if(fs::exists(dep = stem + ".c"))
				comm = vars["CC"] + " " + vars["CFLAGS"];
			else
				continue;
			comm += " -c -o " + target + " " + dep;
			rule.push_impl_dep(std::move(dep));
		}
		// Set implicit command and dependency for executable target
		else {
			dep = target + ".o";
			comm = vars["CC"];

			// No rule for object file; attempt to build from source directly
			if (rules.find(dep) == rules.end()) {
				if (fs::exists(dep = target + ".cpp"))
					comm += " " + vars["CXXFLAGS"];
				else if (fs::exists(dep = target + ".c"))
					comm += " " + vars["CFLAGS"];
				else
					continue;
			}

			comm += " " + vars["LDFLAGS"];
			rule.push_impl_dep(std::move(dep));
			for (const std::string& dep : rule.deps())
				comm += " " + dep;
			comm += " " + vars["LDLIBS"] + " -o " + target;
		}

		rule.commands.push_back(std::move(comm));

	}

	/*
	 * Part 2e: Writing to the cache
	 */

	// Open cache for writing
	xdb::XdbWriter<std::pair<std::string, Rule>> cache(CACHEFILE);

	// First write default target, then everything else
	cache << *default_rule;
	for (const auto& tar_rule : rules)
		if (&tar_rule != default_rule)
			cache << tar_rule;
}

bool Maker::make(const std::string& target, std::unordered_set<std::string> seen) const
{
	/*
	 * Part 2d: Building a target
	 */

	// Check if target already seen in recursion; if so, indicate circular dependency
	if (seen.find(target) != seen.end())
		return false;

	// Mark this target as seen in recursion
	seen.insert(target);

	// Check if target file exists
	bool up_to_date = fs::exists(target);

	// Check if rule exists for target
	auto it = rules.find(target);
	if (it == rules.end()) {
		// If not, check if file exists; if not, throw exception for missing rule
		if (!up_to_date)
			throw exception::TargetNoRule(target);
		else
			throw exception::TargetNothingToDo(target);
	}

	// Iterate over target's dependencies
	const Rule& rule = it->second;
	bool dep_built = false;
	for (const std::string& dep : rule.deps()) {
		try {
			// Attempt to build dependency; set dep_built if no exception is thrown and no circular dep
			if (!make(dep, seen)) {
				// Acknowledge the circular dependency and skip
				std::cerr << "mymake: Circular " << target << " <- "
				          << dep << " dependency dropped." << std::endl;
				continue;
			}
			dep_built = true;
		}
		// Fall through on nonfatal exceptions; dep_built isn't set
		catch (const exception::NonFatal& e) {}

		// Keep track of whether deps are updated to know if target must be re-built
		up_to_date = up_to_date && fs::exists(dep) &&
		             fs::last_write_time(dep) <= fs::last_write_time(target);
	}

	// Nothing to re-build; target is up-to-date
	if (up_to_date && !rule.phony)
		throw exception::TargetUpToDate(target);

	// No commands to execute and no deps built; nothing to do for target
	if (rule.commands.empty() && !dep_built)
		throw exception::TargetNothingToDo(target);

	// Execute commands for target
	for (const std::string& comm : rule.commands) {
		std::cout << comm << std::endl;
		int wstatus = std::system(comm.c_str());
		if (wstatus == -1)
			throw std::system_error(errno, std::generic_category(), "std::system");
		else if (WEXITSTATUS(wstatus) != 0)
			throw exception::TargetCommandFailed(target, WEXITSTATUS(wstatus));
	}

	return true;
}
