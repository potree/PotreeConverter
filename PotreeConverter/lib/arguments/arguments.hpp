
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

using std::unordered_map;
using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

class AValue{
public:
	vector<string> values;

	AValue(vector<string> values) {
		this->values = values;
	}

	template<typename T>
	T as(T alternative) {
		return !values.empty() ? T(values[0]) : alternative;
	}

	template<typename T>
	T as() {
		return !values.empty() ? T(values[0]) : T();
	}

};

template<> vector<string> AValue::as<vector<string>>(vector<string> alternative) {
	return !values.empty() ? values : alternative;
}

template<> vector<string> AValue::as<vector<string>>() {
	return !values.empty() ? values : vector<string>{};
}

template<> vector<double> AValue::as<vector<double>>(vector<double> alternative) {
	vector<double> res;
	for (auto &v : values) {
		res.push_back(std::stod(v));
	}
	return !res.empty() ? res : alternative;
}

template<> vector<double> AValue::as<vector<double>>() {
	return as<vector<double>>({});
}

template<> double AValue::as<double>(double alternative) {
	return !values.empty() ? std::stod(values[0]) : alternative;
}

template<> double AValue::as<double>() {
	return !values.empty() ? std::stod(values[0]) : 0.0;
}

template<> int AValue::as<int>(int alternative) {
	return !values.empty() ? std::stoi(values[0]) : alternative;
}

template<> int AValue::as<int>() {
	return !values.empty() ? std::stoi(values[0]) : 0;
}

class Argument {
private: 

	vector<string> split(string str, vector<char> delimiters) {

		vector<string> tokens;

		auto isDelimiter = [&delimiters](char ch) {
			for (auto &delimiter : delimiters) {
				if (ch == delimiter) {
					return true;
				}
			}

			return false;
		};

		int start = 0;
		for (int i = 0; i < str.size(); i++) {
			if (isDelimiter(str[i])) {
				if (start < i) {
					auto token = str.substr(start, i - start);
					tokens.push_back(token);
				} else {
					tokens.push_back("");
				}

				start = i + 1;
			}
		}

		if (start < str.size()) {
			tokens.push_back(str.substr(start));
		} else if (isDelimiter(str[str.size() - 1])) {
			tokens.push_back("");
		}

		return tokens;
	}

public:
	string id = "";
	string description = "";

	Argument(string id, string description) {
		this->id = id;
		this->description = description;
	}

	bool is(string name) {
		auto tokens = split(id, { ',' });

		for (auto token : tokens) {
			if (token == name) {
				return true;
			}
		}

		return false;
	}

	string fullname() {
		auto tokens = split(id, { ',' });

		for (auto token : tokens) {
			if (token.size() > 1) {
				return token;
			}
		}

		return "";
	}

	string shortname() {
		auto tokens = split(id, { ',' });

		for (auto token : tokens) {
			if (token.size() == 1) {
				return token;
			}
		}

		return "";
	}
};

class Arguments {

private:
	bool startsWith(const string &str, const string &prefix) {
		if (str.size() < prefix.size()) {
			return false;
		}

		return str.substr(0, prefix.size()).compare(prefix) == 0;
	}

public:

	int argc = 0;
	char **argv = nullptr;

	bool ignoreFirst = true;

	vector<string> tokens;
	vector<Argument> argdefs;
	unordered_map<string, vector<string>> map;

	Arguments(int argc, char **argv, bool ignoreFirst = true) {
		this->argc = argc;
		this->argv = argv;
		this->ignoreFirst = ignoreFirst;

		for (int i = ignoreFirst ? 1 : 0; i < argc; i++) {
			string token = string(argv[i]);
			tokens.push_back(token);
		}

		string currentKey = "";
		map.insert({ currentKey, {} });
		for (string token : tokens) {
			if(startsWith(token, "---")) {
				cerr << "Invalid argument: " << token << endl;
				exit(1);
			} else if (startsWith(token, "--")) {
				currentKey = token.substr(2);
				map.insert({ currentKey,{} });
			} else if (startsWith(token, "-")) {
				currentKey = token.substr(1);
				map.insert({ currentKey,{} });
			} else {
				map[currentKey].push_back(token);
			}
		}
	}

	void addArgument(string id, string description) {
		Argument arg(id, description);

		argdefs.push_back(arg);
	}

	Argument *getArgument(string name) {
		for (Argument &arg : argdefs) {
			if (arg.is(name)) {
				return &arg;
			}
		}
		
		return nullptr;
	}

	vector<string> keys() {
		vector<string> keys;
		for (auto entry : map) {
			keys.push_back(entry.first);
		}

		return keys;
	}

	string usage() {
		return "TODO";
	}

	bool has(string name) {
		Argument *arg = getArgument(name);

		if (arg == nullptr) {
			return false;
		}

		for (auto entry : map) {
			if (arg->is(entry.first)) {
				return true;
			}
		}

		return false;
	}

	AValue get(string name) {
		Argument *arg = getArgument(name);

		for (auto entry : map) {
			if (arg->is(entry.first)) {
				return AValue(entry.second);
			}
		}

		return AValue({});
	}




};

