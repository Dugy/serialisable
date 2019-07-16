/*
* \brief Class convenient and duplication-free serialisation and deserialisation of its descendants into JSON, with one function to handle both loading and saving
*
* See more at https://github.com/Dugy/quick_preferences
*/

#ifndef SERIALISABLE_BY_DUGI_HPP
#define SERIALISABLE_BY_DUGI_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <fstream>
#include <exception>
#include <sstream>
#include <type_traits>

class Serialisable {

public:
	enum class JSONtype : uint8_t {
		NIL,
		STRING,
		NUMBER,
		BOOL,
		ARRAY,
		OBJECT
	};
	struct JSON {
		inline virtual JSONtype type() {
			return JSONtype::NIL;
		}
		inline virtual std::string& getString() {
			throw(std::runtime_error("String value is not really string"));
		}
		inline virtual double& getDouble() {
			throw(std::runtime_error("Double value is not really double"));
		}
		inline virtual bool& getBool() {
			throw(std::runtime_error("Bool value is not really bool"));
		}
		inline virtual std::vector<std::shared_ptr<JSON>>& getVector() {
			throw(std::runtime_error("Array value is not really array"));
		}
		inline virtual std::unordered_map<std::string, std::shared_ptr<JSON>>& getObject() {
			throw(std::runtime_error("Object value is not really an object"));
		}
		inline virtual void write(std::ostream& out, int = 0) {
			out << "null";
		}
		inline void writeToFile(const std::string& fileName) {
			std::ofstream out(fileName);
			if (!out.good()) throw(std::runtime_error("Could not write to file " + fileName));
			this->write(out, 0);
		}

		virtual ~JSON() = default;
	protected:
		static void writeString(std::ostream& out, const std::string& written) {
			out.put('"');
			for (unsigned int i = 0; i < written.size(); i++) {
				if (written[i] == '"') {
					out.put('/');
					out.put('"');
				} else if (written[i] == '\n') {
					out.put('\\');
					out.put('n');
				} else if (written[i] == '\\') {
					out.put('\\');
					out.put('\\');
				} else
					out.put(written[i]);
			}
			out.put('"');
		}
		static void indent(std::ostream& out, int depth) {
			for (int i = 0; i < depth; i++)
				out.put('\t');
		}
	};
	struct JSONstring : public JSON {
		std::string contents_;
		JSONstring(const std::string& from = "") : contents_(from) {}

		inline virtual JSONtype type() {
			return JSONtype::STRING;
		}
		inline virtual std::string& getString() {
			return contents_;
		}
		inline void write(std::ostream& out, int = 0) {
			writeString(out, contents_);
		}
	};
	struct JSONdouble : public JSON {
		double value_;
		JSONdouble(double from = 0) : value_(from) {}

		inline virtual JSONtype type() {
			return JSONtype::NUMBER;
		}
		inline virtual double& getDouble() {
			return value_;
		}
		inline void write(std::ostream& out, int = 0) {
			out << value_;
		}
	};
	struct JSONbool : public JSON {
		bool value_;
		JSONbool(bool from = false) : value_(from) {}

		inline virtual JSONtype type() {
			return JSONtype::BOOL;
		}
		inline virtual bool& getBool() {
			return value_;
		}
		inline void write(std::ostream& out, int = 0) {
			out << (value_ ? "true" : "false");
		}
	};
	struct JSONobject : public JSON {
		std::unordered_map<std::string, std::shared_ptr<JSON>> contents_;
		JSONobject() {}

		inline virtual JSONtype type() {
			return JSONtype::OBJECT;
		}
		inline virtual std::unordered_map<std::string, std::shared_ptr<JSON>>& getObject() {
			return contents_;
		}
		inline void write(std::ostream& out, int depth = 0) {
			if (contents_.empty()) {
				out.put('{');
				out.put('}');
				return;
			}
			out.put('{');
			out.put('\n');
			bool first = true;
			for (auto& it : contents_) {
				if (first)
					first = false;
				else {
					out.put(',');
					out.put('\n');
				}
				indent(out, depth + 1);
				writeString(out, it.first);
				out.put(':');
				out.put(' ');
				it.second->write(out, depth + 1);
			}
			out.put('\n');
			indent(out, depth);
			out.put('}');
		}
	};
	struct JSONarray : public JSON {
		std::vector<std::shared_ptr<JSON>> contents_;
		JSONarray() {}

		inline virtual JSONtype type() {
			return JSONtype::ARRAY;
		}
		inline virtual std::vector<std::shared_ptr<JSON>>& getVector() {
			return contents_;
		}
		inline void write(std::ostream& out, int depth = 0) {
			out.put('[');
			if (contents_.empty()) {
				out.put(']');
				return;
			}
			for (auto& it : contents_) {
				out.put('\n');
				indent(out, depth);
				indent(out, depth);
				it->write(out, depth + 1);
			}
			out.put('\n');
			indent(out, depth);
			out.put(']');
			
		}
	};

	static std::shared_ptr<JSON> parseJSON(std::istream& in) {
		auto readString = [&in] () -> std::string {
			char letter = in.get();
			std::string collected;
			while (letter != '"') {
				if (letter == '\\') {
					if (in.get() == '"') collected.push_back('"');
					else if (in.get() == 'n') collected.push_back('\n');
					else if (in.get() == '\\') collected.push_back('\\');
				} else {
					collected.push_back(letter);
				}
				letter = in.get();
			}
			return collected;
		};
		auto readWhitespace = [&in] () -> char {
			char letter;
			do {
				letter = in.get();
			} while (letter == ' ' || letter == '\t' || letter == '\n' || letter == ',');
			return letter;
		};

		char letter = readWhitespace();
		if (letter == 0 || letter == EOF) return std::make_shared<JSON>();
		else if (letter == '"') {
			return std::make_shared<JSONstring>(readString());
		}
		else if (letter == 't') {
			if (in.get() == 'r' && in.get() == 'u' && in.get() == 'e')
				return std::make_shared<JSONbool>(true);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'true'"));
		}
		else if (letter == 'f') {
			if (in.get() == 'a' && in.get() == 'l' && in.get() == 's' && in.get() == 'e')
				return std::make_shared<JSONbool>(false);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'false'"));
		}
		else if (letter == 'n') {
			if (in.get() == 'u' && in.get() == 'l' && in.get() == 'l')
				return std::make_shared<JSON>();
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'null'"));
		}
		else if (letter == '-' || (letter >= '0' && letter <= '9')) {
			std::string asString;
			asString.push_back(letter);
			do {
				letter = in.get();
				asString.push_back(letter);
			} while (letter == '-' || letter == 'E' || letter == 'e' || letter == ',' || letter == '.' || (letter >= '0' && letter <= '9'));
			in.unget();
			std::stringstream parsing(asString);
			double number;
			parsing >> number;
			return std::make_shared<JSONdouble>(number);
		}
		else if (letter == '{') {
			auto retval = std::make_shared<JSONobject>();
			do {
				letter = readWhitespace();
				if (letter == '"') {
					const std::string& name = readString();
					letter = readWhitespace();
					if (letter != ':') throw(std::runtime_error("JSON parser expected an additional ':' somewhere"));
					retval->getObject()[name] = parseJSON(in);
				} else break;
			} while (letter != '}');
			return retval;
		}
		else if (letter == '[') {
			auto retval = std::make_shared<JSONarray>();
			do {
				letter = readWhitespace();
				if (letter == '{') {
					in.unget();
					retval->getVector().push_back(parseJSON(in));
				} else break;
			} while (letter != ']');
			return retval;
		} else {
			throw(std::runtime_error("JSON parser found unexpected character " + letter));
		}
		return std::make_shared<JSON>();
	}
	static std::shared_ptr<JSON> parseJSON(const std::string& fileName) {
		std::ifstream in(fileName);
		if (!in.good()) return std::make_shared<JSON>();
		return parseJSON(in);
	}

private:
	mutable std::shared_ptr<JSON> preferencesJson_;
	mutable bool preferencesSaving_;

protected:
	/*!
	* \brief Should all the synch() method on all members that are to be saved
	*
	* \note If something unusual needs to be done, the saving() method returns if it's being saved or not
	* \note The sync() method knows whether to save it or not
	* \note Const correctness is violated here, but I find it better than duplication
	*/
	virtual void serialisation() = 0;

	/*!
	* \brief Returns if the structure is being saved or loaded
	*
	* \return Whether it's saved (loaded if false)
	*
	* \note Result is meaningless outside a serialisation() overload
	*/
	inline bool saving() {
		return preferencesSaving_;
	}

	/*!
	* \brief Saves or loads a string value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	inline bool synch(const std::string& key, std::string& value) {
		if (preferencesSaving_) {
			preferencesJson_->getObject()[key] = std::make_shared<JSONstring>(value);
		} else {
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				value = found->second->getString();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an arithmetic value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*
	* \note The value is converted from and to a double for JSON conformity
	*/
	template<typename T>
	typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, bool>::type
	synch(const std::string& key, T& value) {
		if (preferencesSaving_) {
			preferencesJson_->getObject()[key] = std::make_shared<JSONdouble>(double(value));
		} else {
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				value = T(found->second->getDouble());
			} return false;
		}
		return true;
	}

	/*!
	* \brief Saves or loads a bool
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	inline bool synch(const std::string& key, bool& value) {
		if (preferencesSaving_) {
			preferencesJson_->getObject()[key] = std::make_shared<JSONbool>(value);
		} else {
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				value = found->second->getBool();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an object derived from Serialisable held in a smart pointer
	* \param The name of the value in the output/input file
	* \param Reference to the pointer
	* \return false if the value was absent while reading, true otherwise
	*
	\ \note The smart pointer class must be dereferencable through operator*(), constructible from raw pointer to the class and the ! operation must result in a number
	* \note If not null, the contents will be overwritten, so raw pointers must be initalised before calling it, but no memory leak will occur
	*/
	template<typename T>
	typename std::enable_if<!std::is_base_of<Serialisable, T>::value
			&& std::is_same<bool, typename std::remove_reference<decltype(std::declval<Serialisable>().synch(std::declval<std::string>(), *std::declval<T>()))>::type>::value
			&& std::is_constructible<T, typename std::remove_reference<decltype(*std::declval<T>())>::type*>::value
			&& std::is_arithmetic<typename std::remove_reference<decltype(!std::declval<T>())>::type>::value , bool>::type
	synch(const std::string& key, T& value) {
		if (preferencesSaving_) {
			if (!value)
				preferencesJson_->getObject()[key] = std::make_shared<JSON>();
			else {
				synch(key, *value);
			}
		} else {
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				if (found->second->type() != JSONtype::NIL) {
					value = std::move(T(new typename std::remove_reference<decltype(*std::declval<T>())>::type()));
					synch(key, *value);
				} else
					value = nullptr;
			} else {
				value = nullptr;
				return false;
			}
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an object derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, T>::value, bool>::type
	synch(const std::string& key, T& value) {
		value.preferencesSaving_ = preferencesSaving_;
		if (preferencesSaving_) {
			auto making = std::make_shared<JSONobject>();
			value.preferencesJson_ = making;
			value.serialisation();
			preferencesJson_->getObject()[key] = making;
			value.preferencesJson_.reset();
		} else {
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				value.preferencesJson_ = found->second;
				value.serialisation();
				value.preferencesJson_.reset();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads a vector of objects derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the vector
	* \return false if the value was absent while reading, true otherwise
	*
	* \note Class must be default constructible
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, T>::value, bool>::type
	synch(const std::string& key, std::vector<T>& value) {
		if (preferencesSaving_) {
			auto making = std::make_shared<JSONarray>();
			for (unsigned int i = 0; i < value.size(); i++) {
				auto innerMaking = std::make_shared<JSONobject>();
				value[i].preferencesSaving_ = true;
				value[i].preferencesJson_ = innerMaking;
				value[i].serialisation();
				value[i].preferencesJson_.reset();
				making->getVector().push_back(innerMaking);
			}
			preferencesJson_->getObject()[key] = making;
		} else {
			value.clear();
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				for (unsigned int i = 0; i < found->second->getVector().size(); i++) {
					value.push_back(T());
					T& filled = value.back();
					filled.preferencesSaving_ = false;
					filled.preferencesJson_ = found->second->getVector()[i];
					filled.serialisation();
					filled.preferencesJson_.reset();
				}
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads a vector of smart pointers to objects derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the vector
	* \return false if the value was absent while reading, true otherwise
	*
	* \note The smart pointer class must be dereferencable through operator*() and constructible from raw pointer to the class
	* \note The class must be default constructible
	* \note Using a vector of raw pointers may cause memory leaks if there is some content before loading
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, typename std::remove_reference<decltype(*std::declval<T>())>::type>::value
			&& std::is_constructible<T, typename std::remove_reference<decltype(*std::declval<T>())>::type*>::value, bool>::type
	synch(const std::string& key, std::vector<T>& value) {
		if (preferencesSaving_) {
			auto making = std::make_shared<JSONarray>();
			for (unsigned int i = 0; i < value.size(); i++) {
				auto innerMaking = std::make_shared<JSONobject>();
				(*value[i]).preferencesSaving_ = true;
				(*value[i]).preferencesJson_ = innerMaking;
				(*value[i]).serialisation();
				(*value[i]).preferencesJson_.reset();
				making->getVector().push_back(innerMaking);
			}
			preferencesJson_->getObject()[key] = making;
		} else {
			value.clear();
			auto found = preferencesJson_->getObject().find(key);
			if (found != preferencesJson_->getObject().end()) {
				for (unsigned int i = 0; i < found->second->getVector().size(); i++) {
					value.emplace_back(new typename std::remove_reference<decltype(*std::declval<T>())>::type());
					T& filled = value.back();
					(*filled).preferencesSaving_ = false;
					(*filled).preferencesJson_ = found->second->getVector()[i];
					(*filled).serialisation();
					(*filled).preferencesJson_.reset();
				}
			} else return false;
		}
		return true;
	}

public:
	/*!
	* \brief Serialises the object to a JSON string
	* \return The JSON string
	*
	* \note It calls the overloaded process() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline std::string serialise() const {
		std::shared_ptr<JSON> target = std::make_shared<JSONobject>();
		preferencesJson_ = target;
		preferencesSaving_ = true;
		const_cast<Serialisable*>(this)->serialisation();
		std::stringstream out;
		preferencesJson_->write(out);
		preferencesJson_ = nullptr;
		return out.str();
	}
	
	/*!
	* \brief Saves the object to a JSON file
	* \param The name of the JSON file
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void save(const std::string& fileName) const {
		preferencesJson_ = std::make_shared<JSONobject>();
		preferencesSaving_ = true;
		const_cast<Serialisable*>(this)->serialisation();
		preferencesJson_->writeToFile(fileName);
		preferencesJson_.reset();
	}

	/*!
	* \brief Loads the object from a JSON string
	* \param The JSON string
	*
	* \note It calls the overloaded process() method
	* \note If the string is blank, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void deserialise(const std::string& source) {
		std::stringstream sourceStream(source);
		std::shared_ptr<JSON> target = parseJSON(source);
		preferencesJson_ = target;
		if (preferencesJson_->type() == JSONtype::NIL) {
			preferencesJson_ = nullptr;
			return;
		}
		preferencesSaving_ = true;
		serialisation();
		preferencesJson_ = nullptr;
	}

	/*!
	* \brief Loads the object from a JSON file
	* \param The name of the JSON file
	*
	* \note It calls the overloaded serialisation() method
	* \note If the file cannot be read, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void load(const std::string& fileName) {
		preferencesJson_ = parseJSON(fileName);
		if (preferencesJson_->type() == JSONtype::NIL) {
			preferencesJson_.reset();
			return;
		}
		preferencesSaving_ = false;
		serialisation();
		preferencesJson_.reset();
	}
};

#endif //SERIALISABLE_BY_DUGI_HPP
