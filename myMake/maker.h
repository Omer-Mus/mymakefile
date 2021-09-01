#pragma once

#include <ostream>
#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

namespace mymake {

/*
 * DO NOT REMOVE any existing data members of Rule or Maker.
 */

struct Rule {
	bool phony = false, implicit_dep = false;
	std::deque<std::string> commands;

	const std::deque<std::string>& deps() const { return deps_dq; }

	// This interface guarantees that dependencies are
	// ordered and unique.
	void push_dep(std::string dep)
	{
		auto ins = deps_set.insert(std::move(dep));
		if (!ins.second)
			return;
		deps_dq.push_back(*ins.first);
	}
	// Add implicit dependency to the front,
	// sets implicit_dep if successful.
	void push_impl_dep(std::string dep)
	{
		auto ins = deps_set.insert(std::move(dep));
		if (!ins.second)
			return;
		implicit_dep = true;
		deps_dq.push_front(*ins.first);
	}

private:
	std::deque<std::string> deps_dq;
	std::unordered_set<std::string> deps_set;
};

class Maker {
public:
	Maker();

	void make() const
	{
		make(default_rule->first);
	}
	void make(const std::string& target) const
	{
		make(target, {});
	}

	friend std::ostream& operator<<(std::ostream& output, const Maker& maker);

private:
	// Internal recursive overload of `make()`;
	// tracks `seen` targets to catch circular dependencies,
	// returns bool to indicate whether circular dependency was found.
	bool make(const std::string& target, std::unordered_set<std::string> seen) const;

	// `rules` maps targets to rules.
	// `default_rule` points to the key-value pair in `rules` for the default target.
	std::unordered_map<std::string, Rule> rules;
	std::pair<const std::string, Rule> *default_rule = nullptr;
};

inline std::ostream& operator<<(std::ostream& output, const Maker& maker)
{
	auto put_tar_rule = [&output](const std::pair<const std::string, Rule>& tar_rule)
	{
		const Rule& rule = tar_rule.second;
		if (rule.phony)
			output << "(phony)" << '\n';
		output << tar_rule.first << ": ";
		for (const std::string& dep : rule.deps())
			output << dep << " ";
		output << '\n';
		for (const std::string& comm : rule.commands)
			output << '\t' << comm << '\n';
		output << '\n';
	};

	put_tar_rule(*maker.default_rule);
	for (const auto& tar_rule : maker.rules)
		if (&tar_rule != maker.default_rule)
			put_tar_rule(tar_rule);

	return output;	
}

/*
 * Part 2e: Serializing a target-rule pair
 */

inline std::ofstream& operator<<(std::ofstream& output, const std::pair<std::string, Rule>& tar_rule)
{
	output << tar_rule.first << '\0';

	const Rule& rule = tar_rule.second;

	output << static_cast<char>(rule.phony);
	output << static_cast<char>(rule.implicit_dep);

	for (const std::string& dep : rule.deps())
		output << dep << '\0';
	output << '\0';
	for (const std::string& comm : rule.commands)
		output << comm << '\0';
	output << '\0';

	return output;
}

inline std::ifstream& operator>>(std::ifstream& input, std::pair<std::string, Rule>& tar_rule)
{
	std::getline(input, tar_rule.first, '\0');

	Rule& rule = tar_rule.second = Rule();

	char val;
	input >> val;
	rule.phony = static_cast<bool>(val);
	input >> val;
	rule.implicit_dep = static_cast<bool>(val);

	std::string item;
	while (std::getline(input, item, '\0') && !item.empty())
		rule.push_dep(std::move(item));
	while (std::getline(input, item, '\0') && !item.empty())
		rule.commands.push_back(std::move(item));

	return input;
}

} // namespace mymake
