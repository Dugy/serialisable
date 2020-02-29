#ifndef SERIALISABLE_BETTER_ENUM_HPP
#define SERIALISABLE_BETTER_ENUM_HPP
#include "better-enums/enum.h"
#include "serialisable.hpp"

BETTER_ENUM(SuperEnum, int, AAAA, BBBB);

namespace SerialisableInternals {
template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_same<decltype(std::declval<Serialised>()._to_string()), const char*>::value
		&& std::is_same<decltype(Serialised::_from_string(std::declval<const char*>())), Serialised>::value
		&& std::is_integral<decltype(std::declval<Serialised>()._to_integral())>::value
		&& std::is_same<decltype(Serialised::_from_integral(0)), Serialised>::value>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a better enum value into a JSON string
	* \param The better enum
	* \return The constructed JSON string
	* if the value is single precision, the precision of condensed JSON will be guessed
	*/
	static Serialisable::JSON serialise(Serialised value) {
		return value._to_string();
	}
	/*!
	* \brief Loads a JSON string into a better enum
	* \param Reference to the result value
	* \param The JSON string
	* \throw If the something's wrong with the JSON
	*/
	static void deserialise(Serialised& result, const Serialisable::JSON& value) {
		result = Serialised::_from_string(value.string().c_str());
	}
};

}

#endif // SERIALISABLE_BETTER_ENUM_HPP
