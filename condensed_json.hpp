#ifndef CONDENSED_JSON_BY_DUGI_HPP
#define CONDENSED_JSON_BY_DUGI_HPP
#include "serialisable.hpp"
#include <iostream>

// A flag allowing to adjust the default precision when using the condensed format. Possibilities:
// HALF_PRECISION - 15 bit float (almost half-precision), 1 bit is sign, 6 exponent, 8 mantissa, the imprecision is about 0.2%, maximal value is in the order of ten power 9
// SINGLE_PRECISION - regular float
// DOUBLE_PRECISION - regular double
// Default is HALF_PRECISION
#ifndef SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION
#define SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION HALF_PRECISION
#endif

class CondensedJSON {


	struct CondensedInfo {
		enum CondensedPrefix : uint8_t {
			HALF_PRECISION_FLOAT = 0b10000000,
			SHORT_STRING = 0b01100000,
			RESERVED_1 = 0b01111110,
			LONG_STRING = 0b01111111,
			MINIMAL_INTEGER = 0b01000000,
			COMMON_OBJECT = 0b00111000,
			RESERVED_2 = 0b00111101,
			UNCOMMON_OBJECT = 0b00111110,
			RARE_OBJECT = 0b00111111,
			SMALL_UNIQUE_OBJECT = 0b00110000,
			LARGE_UNIQUE_OBJECT = 0b00110110,
			HASHTABLE= 0b00110111,
			SHORT_ARRAY = 0b00100000,
			RESERVED_3 = 0b00101101,
			LONG_ARRAY = 0b00101111,
			VERY_SHORT_INTEGER = 0b00010000,
			DOUBLE = 0x0f,
			FLOAT = 0x0e,
			SIGNED_LONG_INTEGER = 0x0d,
			UNSIGNED_LONG_INTEGER = 0x0c,
			SIGNED_INTEGER = 0x0b,
			UNSIGNED_INTEGER = 0x0a,
			SIGNED_SHORT_INTEGER = 0x09,
			UNSIGNED_SHORT_INTEGER = 0x08,
			RESERVED_4 = 0x04,
			TRUE = 0x03,
			FALSE = 0x02,
			NIL = 0x01,
			TERMINATOR = 0x00,
		};
		constexpr static int MAX_SHORT_STRING_SIZE = 30;
		constexpr static int HALF_FLOAT_EXPONENT_BITS = 6;
		constexpr static int HALF_FLOAT_MANTISSA_BITS = 8;
		constexpr static int MAX_SHORT_ARRAY_SIZE = 14;
		constexpr static int MAX_COMMON_OBJECT_ID = 5;
		constexpr static int MAX_UNCOMMON_OBJECT_ID = MAX_COMMON_OBJECT_ID + 1 + 0xff;
		constexpr static int MAX_SMALL_UNIQUE_OBJECT_SIZE = 6;
		constexpr static uint8_t STRING_FINAL_BIT_FLIP = 0x80;

		constexpr static int HALF_PRECISION_FLOAT_MASK = 0x7f;
		constexpr static int SHORT_STRING_MASK = 0x1f;
		constexpr static int MINIMAL_INTEGER_MASK = 0x1f;
		constexpr static int MINIMAL_INTEGER_NUMBER_MASK = 0x0f;
		constexpr static int MINIMAL_INTEGER_SIGN_MASK = 0x10;
		constexpr static int OBJECT_MASK = 0x07;
		constexpr static int SHORT_ARRAY_MASK = 0x0f;
		constexpr static int VERY_SHORT_INTEGER_MASK = 0x0f;
		constexpr static int VERY_SHORT_INTEGER_SIGN_MASK = 0x08;
		constexpr static int VERY_SHORT_INTEGER_PREFIX_MASK = 0x07;
		constexpr static int RESERVED_4_MASK = 0x03;
	};

	struct ObjectMapEntry {
		int index;
		bool used = false;
	};

	using JSON = Serialisable::JSON;
	using String = Serialisable::JSON::String;

public:
	static std::vector<uint8_t> serialise(const JSON& source) {
		std::vector<uint8_t> result;
		auto mapping = generateObjectMapping(source);
		writeCondensed(source, result, mapping);
		return result;
	}

	static JSON deserialise(const std::vector<uint8_t>& source) {
		const uint8_t* data = source.data() - 1;
		std::vector<std::unique_ptr<std::vector<std::string>>> objects;
		return parseCondensed(data, source.data() + source.size(), objects);
	}
private:
	static JSON parseCondensed(uint8_t const*& source, const uint8_t* end, std::vector<std::unique_ptr<std::vector<std::string>>>& objects) {
		// Recursion invariant: source always points to 1 byte before the start of the object
		auto next = [&] () {
			source++;
			if (source >= end)
				throw std::runtime_error("Condensed JSON got to an unexpected end of data");
		};
		auto peek = [&] () {
			if (source + 1 >= end)
				throw std::runtime_error("Condensed JSON got to an unexpected end of data");
			return *(source + 1);
		};
		auto parseInt = [&] (auto typeIdentificator) {
			decltype(typeIdentificator) made = 0;
			for (int i = 0; i < int(sizeof(typeIdentificator)) * 8; i += 8) {
				next();
				made |= uint64_t(*source) << i;
			}
			return made;
		};
		auto readCodeString = [&] () {
			next();
			if (*source == CondensedInfo::STRING_FINAL_BIT_FLIP)
				return std::string("");

			std::string made;
			while (true) {
				if (*source < CondensedInfo::STRING_FINAL_BIT_FLIP) {
					made.push_back(*source);
					next();
				} else {
					made.push_back(*source & 0x7f);
					return made;
				}
			}
		};
		auto parseObjectUsingDict = [&] (const std::vector<std::string>& names) {
			JSON made;
			made.setObject();
			for (const std::string& it : names) {
				made[it] = parseCondensed(source, end, objects);
			}
			return made;
		};
		auto parseObject = [&] (const int index) {
			if (int(objects.size()) < index + 1)
				objects.resize(index + 1);
			if (!objects[index]) {
				objects[index] = std::make_unique<std::vector<std::string>>();
				while (peek() != CondensedInfo::TERMINATOR)
					objects[index]->push_back(readCodeString());
				next();
			}
			return parseObjectUsingDict(*objects[index]);
		};

		next();
		if (*source & CondensedInfo::HALF_PRECISION_FLOAT) {
			uint64_t result = (*source & 0x40ull) << 57; // Sign
			result |= (0x3e0 + (*source & 0x3full)) << 52; // Exponent
			next();
			result |= uint64_t(*source) << 44; // Mantissa
			return JSON(*reinterpret_cast<double*>(&result));
		} else if (*source == CondensedInfo::LONG_STRING) {
			std::string made;
			next();
			while (*source) {
				made.push_back(*source);
				next();
			};
			return JSON(made);
		} else if (*source == CondensedInfo::RESERVED_1) {
			throw(std::runtime_error("Condensed JSON version is too low"));
		} else if ((*source & 0b11100000) == CondensedInfo::SHORT_STRING) {
			std::string made;
			int length = *source & CondensedInfo::SHORT_STRING_MASK;
			for (int i = 0; i < length; i++) {
				next();
				made.push_back(*source);
			}
			return JSON(made);
		} else if ((*source & 0b11100000) == CondensedInfo::MINIMAL_INTEGER) {
			int64_t made = (*source & CondensedInfo::MINIMAL_INTEGER_NUMBER_MASK);
			if (*source & CondensedInfo::MINIMAL_INTEGER_SIGN_MASK)
				made |= 0xfffffffffffffff0;
			return JSON(made);
		} else if (*source == CondensedInfo::RESERVED_2) {
			throw(std::runtime_error("Condensed JSON version is too low"));
		} else if (*source == CondensedInfo::UNCOMMON_OBJECT) {
			next();
			int index = *source + CondensedInfo::MAX_COMMON_OBJECT_ID;
			return parseObject(index);
		} else if (*source == CondensedInfo::RARE_OBJECT) {
			next();
			int index = *source << 8;
			next();
			index += *source + CondensedInfo::MAX_UNCOMMON_OBJECT_ID;
			return parseObject(index);
		} else if ((*source & CondensedInfo::COMMON_OBJECT) == CondensedInfo::COMMON_OBJECT) {
			int index = *source & CondensedInfo::OBJECT_MASK;
			return parseObject(index);
		} else if (*source == CondensedInfo::LARGE_UNIQUE_OBJECT) {
			std::vector<std::string> names;
			while (peek() != CondensedInfo::TERMINATOR) {
				names.push_back(readCodeString());
			}
			next();
			return parseObjectUsingDict(names);
		} else if (*source == CondensedInfo::HASHTABLE) {
			std::vector<std::string> names;
			next();
			while (*source != CondensedInfo::TERMINATOR) {
				std::string made;
				while (*source != CondensedInfo::TERMINATOR) {
					made.push_back(*source);
					next();
				}
				next();
				names.push_back(made);
			}
			if (peek() == CondensedInfo::TERMINATOR) {
				names.push_back("");
				next();
			}
			return parseObjectUsingDict(names);
		} else if ((*source & 0xf0) == CondensedInfo::SMALL_UNIQUE_OBJECT) {
			int size = *source & CondensedInfo::OBJECT_MASK;
			std::vector<std::string> names;
			for (int i = 0; i < size; i++)
				names.push_back(readCodeString());
			return parseObjectUsingDict(names);
		} else if (*source == CondensedInfo::LONG_ARRAY) {
			JSON made;
			made.setArray();
			while (peek() != CondensedInfo::TERMINATOR)
				made.push_back(parseCondensed(source, end, objects));
			made.array().shrink_to_fit();
			return made;
		} else if ((*source & 0xf0) == CondensedInfo::SHORT_ARRAY) {
			JSON made;
			int size = *source & CondensedInfo::SHORT_ARRAY_MASK;
			made.setArray().reserve(size);
			for (int i = 0; i < size; i++)
				made.push_back(parseCondensed(source, end, objects));
			return made;
		} else if ((*source & 0xf0) == CondensedInfo::VERY_SHORT_INTEGER) {
			int64_t made;
			made = (*source & CondensedInfo::VERY_SHORT_INTEGER_PREFIX_MASK) << 8;
			if (*source & CondensedInfo::VERY_SHORT_INTEGER_SIGN_MASK)
				made |= 0xfffffffffffff800;
			next();
			made |= *source;
			return JSON(made);
		} else if (*source == CondensedInfo::DOUBLE) {
			uint64_t buffer = 0;
			for (int i = 0; i < int(sizeof(double)) * 8; i += 8) {
				next();
				buffer |= uint64_t(*source) << i;
			}
			return JSON(*reinterpret_cast<double*>(&buffer));
		} else if (*source == CondensedInfo::FLOAT) {
			uint32_t buffer = 0;
			for (int i = 0; i < int(sizeof(float)) * 8; i += 8) {
				next();
				buffer |= uint32_t(*source) << i;
			}
			return JSON(*reinterpret_cast<float*>(&buffer));
		} else if (*source == CondensedInfo::SIGNED_LONG_INTEGER) {
			return JSON(parseInt(int64_t()));
		} else if (*source == CondensedInfo::UNSIGNED_LONG_INTEGER) {
			return JSON(parseInt(uint64_t()));
		} else if (*source == CondensedInfo::SIGNED_INTEGER) {
			return JSON(parseInt(int32_t()));
		} else if (*source == CondensedInfo::UNSIGNED_INTEGER) {
			return JSON(parseInt(uint32_t()));
		} else if (*source == CondensedInfo::SIGNED_SHORT_INTEGER) {
			return JSON(parseInt(int16_t()));
		} else if (*source == CondensedInfo::UNSIGNED_SHORT_INTEGER) {
			return JSON(parseInt(uint16_t()));
		} else if (*source == CondensedInfo::TRUE) {
			return JSON(true);
		} else if (*source == CondensedInfo::FALSE) {
			return JSON(false);
		} else if (*source == CondensedInfo::NIL) {
			return JSON();
		} else if (*source == CondensedInfo::TERMINATOR) {
			throw Serialisable::SerialisationError("Condensed JSON stumbled upon an unexpected ending symbol");
		}
		throw Serialisable::SerialisationError("Condensed JSON failed to recognise type information: " + std::to_string(*source) + " after " + std::to_string(*(source - 1)));
	}


	static void writeCondensed(const JSON& source, std::vector<uint8_t>& buffer, std::unordered_map<std::string, ObjectMapEntry>& mapping) {
		switch(source.type()) {
		case JSON::Type::NIL:
			buffer.push_back(CondensedInfo::NIL);
			return;
		case JSON::Type::STRING: {
			std::string contents = source.string();
			if (contents.size() < CondensedInfo::MAX_SHORT_STRING_SIZE) {
				buffer.push_back(CondensedInfo::SHORT_STRING + contents.size());
				for (auto c : contents) {
					buffer.push_back(static_cast<uint8_t>(c));
				}
			} else {
				buffer.push_back(CondensedInfo::LONG_STRING);
				for (auto c : contents) {
					buffer.push_back(static_cast<uint8_t>(c));
				}
				buffer.push_back(CondensedInfo::TERMINATOR);
			}
			return;
		}
		case JSON::Type::NUMBER: {
			double value = source.number();
			int64_t valueInt = value;
			if (value == valueInt) {
				// It is an integer
				auto writeBinary = [&buffer] (uint8_t lead, auto number) {
					buffer.push_back(lead);
					for (int i = 0; i < int(sizeof(number)) * 8; i += 8)
						buffer.push_back((number >> i) & 0xffull);
				};
				if (valueInt <= 15 && valueInt >= -16) {
					buffer.push_back((static_cast<int8_t>(valueInt) & (CondensedInfo::MINIMAL_INTEGER_MASK)) | CondensedInfo::MINIMAL_INTEGER);
				} else if (valueInt <= 2047 && valueInt >= -2048) {
					buffer.push_back(CondensedInfo::VERY_SHORT_INTEGER | ((valueInt & 0x0f00) >> 8));
					buffer.push_back(valueInt & 0xff);
				} else if (valueInt < std::numeric_limits<int16_t>::max() && valueInt > std::numeric_limits<int16_t>::min())
					writeBinary(CondensedInfo::SIGNED_SHORT_INTEGER, static_cast<int16_t>(valueInt));
				else if (valueInt < std::numeric_limits<uint16_t>::max() && valueInt > std::numeric_limits<uint16_t>::min())
					writeBinary(CondensedInfo::UNSIGNED_SHORT_INTEGER, static_cast<uint16_t>(valueInt));
				else if (valueInt < std::numeric_limits<int32_t>::max() && valueInt > std::numeric_limits<int32_t>::min())
					writeBinary(CondensedInfo::SIGNED_INTEGER, static_cast<int32_t>(valueInt));
				else if (valueInt < std::numeric_limits<uint32_t>::max() && valueInt > std::numeric_limits<uint32_t>::min())
					writeBinary(CondensedInfo::UNSIGNED_INTEGER, static_cast<uint32_t>(valueInt));
				else if (valueInt < std::numeric_limits<int64_t>::max() && valueInt > std::numeric_limits<int64_t>::min())
					writeBinary(CondensedInfo::SIGNED_LONG_INTEGER, static_cast<int64_t>(valueInt));
			} else {
				// Is floating point
				enum class Hint : uint8_t {
					HALF_PRECISION,
					SINGLE_PRECISION,
					DOUBLE_PRECISION,
				};

				Hint hint = Hint::DOUBLE_PRECISION;
				uint64_t triedBinary = *reinterpret_cast<const uint64_t*>(&value);
				double tried = fabs(value);
				constexpr Hint preferred = Hint::SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION;
				if (tried > std::numeric_limits<float>::max() || (tried < std::numeric_limits<float>::min() && tried > 0))
					hint = Hint::DOUBLE_PRECISION;
				else if (preferred != Hint::DOUBLE_PRECISION || float(tried) == tried || (triedBinary & 0x00000000fffffffc)) {
					// Either double is not preferred or it won't lose precision anyway or there are a lot of trailing blank bits
					constexpr float MAX_HALF_PRECISION = 8.57316e+09;
					constexpr float MIN_HALF_PRECISION_POSITIVE = 9.34961e-10;
					if (tried > MAX_HALF_PRECISION || (tried < MIN_HALF_PRECISION_POSITIVE && tried > 0))
						hint = Hint::SINGLE_PRECISION;
					else if (preferred == Hint::HALF_PRECISION || (triedBinary & 0x007ffffffffffffc)) {
						// Either float is not preferred or there are really a lot of trailing blank bits
						hint = Hint::HALF_PRECISION;
					} else hint = Hint::SINGLE_PRECISION;
				}

				if (hint == Hint::HALF_PRECISION) {
					uint64_t source = *reinterpret_cast<const uint64_t*>(&value);
					uint8_t result = 0x80 | ((source & 0x8000000000000000) >> 57); // Identification prefix and sign (1 + 1 bits)
					result |= ((source & 0x7ff0000000000000) >> 52) - 0x3e0; // Exponent (6 bits)
					buffer.push_back(result);
					buffer.push_back((source & 0x000fffffffffffff) >> 44); // Mantissa (1 byte)
				} else if (hint == Hint::SINGLE_PRECISION) {
					buffer.push_back(CondensedInfo::FLOAT);
					float asFloat = float(value);
					uint32_t extractor = *reinterpret_cast<const uint32_t*>(&asFloat);
					for (int i = 0; i < int(sizeof(float)) * 8; i += 8) {
						buffer.push_back((extractor & (0xff << i)) >> i);
					}
				} else if (hint == Hint::DOUBLE_PRECISION) {
					buffer.push_back(CondensedInfo::DOUBLE);
					uint64_t extractor = *reinterpret_cast<const uint64_t*>(&value);
					for (int i = 0; i < int(sizeof(double)) * 8; i += 8)
						buffer.push_back((extractor & (0xffull << i)) >> i);
				}
			}
			return;
		}
		case JSON::Type::BOOL:
			if (source.boolean())
				buffer.push_back(CondensedInfo::TRUE);
			else
				buffer.push_back(CondensedInfo::FALSE);
			return;
		case JSON::Type::OBJECT: {
			auto contents = source.object();
			if (contents.empty()) {
				buffer.push_back(CondensedInfo::SMALL_UNIQUE_OBJECT); // Does not need to be saved
				return;
			}
			std::pair<std::string, bool> descriptor = getDescriptor(contents);
			if (descriptor.second) {
				auto found = mapping.find(descriptor.first);
				if (found != mapping.end()) {
					int index = found->second.index;
					if (index <= CondensedInfo::MAX_COMMON_OBJECT_ID)
						buffer.push_back(CondensedInfo::COMMON_OBJECT | index);
					else if (index <= CondensedInfo::MAX_UNCOMMON_OBJECT_ID) {
						index -= CondensedInfo::MAX_COMMON_OBJECT_ID + 1;
						buffer.push_back(CondensedInfo::UNCOMMON_OBJECT);
						buffer.push_back(index);
					} else {
						index -= CondensedInfo::MAX_UNCOMMON_OBJECT_ID + 1;
						buffer.push_back(CondensedInfo::UNCOMMON_OBJECT);
						buffer.push_back(index >> 8);
						buffer.push_back(index & 0xff);
					}
					if (!found->second.used) {
						if (!descriptor.first.empty()) {
							for (auto c : descriptor.first)
								buffer.push_back(c);
							buffer.push_back(CondensedInfo::TERMINATOR);
							found->second.used = true;
						}
					}
				} else {
					if (contents.size() < CondensedInfo::MAX_SMALL_UNIQUE_OBJECT_SIZE) {
						buffer.push_back(CondensedInfo::SMALL_UNIQUE_OBJECT | contents.size());
						for (auto c : descriptor.first)
							buffer.push_back(c);
					} else {
						buffer.push_back(CondensedInfo::LARGE_UNIQUE_OBJECT);
						for (auto c : descriptor.first)
							buffer.push_back(c);
						buffer.push_back(CondensedInfo::TERMINATOR);
					}
				}

				auto ordered = getOrdered(contents);
				for (auto& it : ordered) {
					writeCondensed(it.second, buffer, mapping);
				}
			} else {
				buffer.push_back(CondensedInfo::HASHTABLE);
				for (auto& it : contents) {
					std::string name = it.first;
					if (!name.empty()) {
						for (auto c : name)
							buffer.push_back(c);
						buffer.push_back(CondensedInfo::TERMINATOR);
					}
				}
				if (contents.find("") != contents.end()) // Empty string must go last
					buffer.push_back(CondensedInfo::TERMINATOR);
				buffer.push_back(CondensedInfo::TERMINATOR);
				for (auto& it : contents)
					if (!std::string(it.first).empty()) {
						writeCondensed(it.second, buffer, mapping);
					}
				auto empty = contents.find("");
				if (empty != contents.end())
					writeCondensed(empty->second, buffer, mapping);
			}
			return;
		}
		case JSON::Type::ARRAY: {
			const auto& contents = source.array();
			if (contents.size() < CondensedInfo::MAX_SHORT_ARRAY_SIZE) {
				buffer.push_back(CondensedInfo::SHORT_ARRAY | contents.size());
				for (auto& it : contents)
					writeCondensed(it, buffer, mapping);
			} else {
				buffer.push_back(CondensedInfo::LONG_ARRAY);
				for (auto& it : contents)
					writeCondensed(it, buffer, mapping);
				buffer.push_back(CondensedInfo::TERMINATOR);
			}
			return;
			}
		default:
			throw Serialisable::SerialisationError("Memory-corrupted JSON");
		}
	}

	static std::unordered_map<std::string, ObjectMapEntry> generateObjectMapping(const JSON& mapped) {
		std::unordered_map<String, int, Serialisable::JSON::StringHasher> list;
		addToObjectList(mapped, list);

		std::vector<std::pair<std::string, int>> ordered;
		for (auto& it : list) {
			ordered.push_back(std::move(it));
		}
		std::sort(ordered.begin(), ordered.end(), [] (std::pair<std::string, int> first, std::pair<std::string, int> second) {
			return first.second > second.second;
		});

		std::unordered_map<std::string, ObjectMapEntry> result;
		for (unsigned int i = 0; i < ordered.size(); i++) {
			if (ordered[i].second <= 1) break; // Objects with a single occurrence are not saved this way
			if (i > 0xffff + CondensedInfo::MAX_UNCOMMON_OBJECT_ID) break; // Cannot batch so many object types
			result[ordered[i].first] = { int(i) };
		}
		return result;
	}

	static void addToObjectList(const JSON& mapped, std::unordered_map<String, int, Serialisable::JSON::StringHasher>& list) {
		if (mapped.type() == JSON::Type::OBJECT) {
			auto& contents = mapped.object();
			if (contents.empty())
				return;
			std::pair<std::string, bool> descriptor = getDescriptor(contents);
			if (descriptor.second)
				list[descriptor.first]++;

			for (auto& it : contents)
				addToObjectList(it.second, list);
		}
	}

	static std::vector<std::pair<String, JSON>> getOrdered(const JSON::ObjectType& mapped) {
		// They must be sorted in order to notice identical objects
		std::vector<std::pair<String, JSON>> ordered;
		for (auto& it : mapped)
			ordered.push_back(it);
		std::sort(ordered.begin(), ordered.end(), [] (auto first, auto second) {
			return std::string(first.first) < std::string(second.first);
		});
		return ordered;
	}
	static std::pair<std::string, bool> getDescriptor(const JSON::ObjectType& object) {
		std::string composed;
		auto ordered = getOrdered(object);
		for (auto& it : ordered) {
			std::string field = it.first;
			if (field.empty()) {
				composed.push_back(CondensedInfo::STRING_FINAL_BIT_FLIP);
			}
			for (unsigned int i = 0; i < field.size(); i++) {
				if (field[i] > 0) {
					if (i < field.size() - 1)
						composed.push_back(field[i]);
					else
						composed.push_back(uint8_t(field[i]) | CondensedInfo::STRING_FINAL_BIT_FLIP);
				} else {
					return { "", false };
				}
			}
		}
		return { composed, true };
	}

};

#endif // CONDENSED_JSON_BY_DUGI
