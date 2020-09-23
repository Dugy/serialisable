# Serialisable
A small header-only library for convenient serialisation of C++ classes without boilerplate. It uses the [JSON format](https://en.wikipedia.org/wiki/JSON). It's designed for minising the amount of code used for saving and loading while keeping the saved file human-readable, it's not optimised for performance.

Normally, persistent preferences tend to have one method for saving that contains code for loading all the content and one method for saving it all. Also, there might be some need for validity checking. This tool allows saving it all in one method that calls overloads of one method to all members to be saved.

It contains a small JSON library to avoid a dependency on a library that is probably much larger than this one. It's not advised to be used for generic JSON usage.

## Usage

Have your classes inherit from the Serialisable class. They have to implement a `serialisation()` method that calls overloads of the `synch()` method that accepts name of the value in the file as first argument and the value (taken as reference) as the second one. If something needs to be processed before saving or after loading, the `saving()` method will return a bool value telling if it's being saved.

```C++
struct Chapter : public Serialisable {
	std::string contents = "";
	std::string author = "Anonymous";

	virtual void serialisation() {
		synch("contents", contents);
		synch("author", author);
	}
};

struct Preferences : public Serialisable {
	std::string lastFolder = "";
	unsigned int lastOpen = 0;
	bool privileged = false;
	Chapter info;
	std::vector<Chapter> chapters;
	std::vector<std::shared_ptr<Chapter>> footnotes;
	std::vector<std::unique_ptr<Chapter>> addenda;

	virtual void serialisation() {
		synch("last_folder", lastFolder);
		synch("last_open", lastOpen);
		synch("privileged", privileged);
		synch("info", info);
		synch("chapters", chapters);
		synch("footnotes", footnotes);
		synch("addenda", addenda);
	}
};

//...

// Loading
Preferences prefs;
prefs.load("prefs.json");
  
// Saving
prefs.save("prefs.json");
```

Supported types are:
* `std::string`
* floating point types (all stored as `double`)
* integer types (all stored as `long int`, mostly indistinguishable from `double` in JSON)
* `bool`
* any object derived from `Serialisable`
* a `std::vector` of types that are serialisable themselves
* an `std::unordered_map` of types that are serialisable themselves, indexed by `std::string`
* smart pointers to otherwise serialisable types (`null` in JSON stands for `nullptr`)
* `std::vector<uint8_t>` representing general binary data (stored in string, base64 encoded)
* The internal JSON format
* `std::optinal` (if C++17 is available)

All of these apply recursively, so it's possible to serialise a `std::shared_ptr<std::unordered_map<std::string, std::vector<int>>>`. It is also possible to enable serialisation of other types.

Default values should be set somewhere, because if a value is missing or `load()` does not find the specified file, it does not call the `serialisation()` method. Missing keys will simply not load the values. Values of wrong types will throw.

It relies only on standard libraries, so you can use any C++14 compliant compiler to compile it.

## JSON library

The JSON library provided is only to avoid having additional dependencies. It's written to be short, its usage is prone to result in repetitive code. If you need JSON for something else, use a proper JSON library, like [the one written by Niels Lohmann](https://github.com/nlohmann/json), they are much more convenient.

If you really need to use it, for example if you are sure you will not use it much, here is an example:

``` C++
	Serialisable::JSON testJson;
	testJson.setObject()["file"] = "test.json";
	testJson["number"] = 9;
	testJson["float_number"] = 9.0;
	testJson["makes_sense"] = false;
	Serialisable::JSON array;
	array.setArray();
	for (int i = 0; i < 3; i++) {
		Serialisable::JSON obj;
		obj.setObject()["index"] = i;
		Serialisable::JSON obj2;
		obj2.setObject()["empty"] = Serialisable::JSON();
		obj["contents"] = obj2;
		array.push_back(obj);
	}
	testJson["data"] = array;
	testJson.save("test.json");

	Serialisable::JSON testReadJson = Serialisable::JSON::load("test.json");
	testReadJson["makes_sense"] = true;
	testReadJson["number"] = 42;
	testReadJson["float_number"] = 4.9;
	testReadJson.save("test-reread.json");
```

The `JSON` object uses the nan-boxing technique to be very space efficient with high locality. Its size is the same as the size of a `double` and can store numbers, bools, nulls, short strings locally or pointers to longer strings, arrays or hashtables. Moving it and copying it is cheap, because string values are copy on write and arrays and hashtables are not copied (they are reference counted). Although this is mostly for convenience, its copying behaviour is identical to JavaScript data structures.

The internal type can be checked using its `type()` method. The contents can be accessed using the right getter/setter, such as `number()`, `boolean()` etc. Assignment or implicit conversion can be used too. Operator `[]` is overloaded for strings and numbers to shorten access to arrays and hashtables. `push_back()` and `size()` can also be accessed directly without calling the `array()` or `object()` getters. If contents of an incorrect type are accessed, an exception is thrown. In order to set the type to array or object, use the `setArray()` and `setObject()` methods respectively (they also work as getters).

The parser can parse incorrect code in some cases because some of the information in JSON files is redundant.

If necessary, `JSON` can also be a serialised member if the `synch` method is used on it.

## Optional extensions

This tool can be extended with custom data types to be serialised and custom ways to save the data into files (however, they must be representable as JSON).

Here are some optional extensions that come with this repository.

### SerialisableBrief - write even less code

To write even less code for serialisation, you can use `SerialisableBrief`, a façade above `Serialisable`. It's in its separate header file, `serialisable_brief.hpp`.

It allows serialising with even less code:
```C++
struct Chapter : public SerialisableBrief {
	std::string contents = key("contents");
	std::string author = key("author") = "Anonymous";
	int readers = key("read_by") = 0;
};
```
It does the same as the `Chapter` class in the section above. The members must be initialised with the `key()` method, whose argument is the key of the field in JSON. The members can be optionally initialised for real by assigning into the result of the `key()` method call.

The members can also be initialised using the `init()` method that takes any number of arguments that will be fed to the constructor (if not used, it will be default-initialised; brace-enclosed initialisers cannot be used). This can be shortened by adding any number of arguments after the first argument to the `key()` method, the additional arguments will be given to the constructor. This uses `Serialisable` to actually convert the variables to JSON, so it can serialise the same types as `Serialisable` can.

```C++
struct ChapterInfo : public SerialisableBrief {
	std::unordered_set<std::string> currentReaders = skip();
	int pages = key("pages").init(100);
	std::string summary = key("summary", "Some interesting stuff");
};
```

**Important:** if a member is *not* to be serialised, it has to be initialised with the `skip()` method (can be initialised the same way as with `key()`; it's usable also for types that cannot be serialised by `Serialisable`). Otherwise, undefined behaviour is very likely to occur when serialising/deserialising the next member, without any warning (there is a detection mechanism that throws exceptions in constructor, but it's not reliable). This applies also to any classes that inherit from it unless none of them uses any further serialisation. Therefore, this should be used only for classes that hold data and don't have much other functionality. You have been warned.

The cost of this brevity is proneness to human errors, obscure code and lower performance, especially when constructing the objects. To avoid forgetting the `skip()` method, it's better to use `Serialisable` instead for more complex classes that aren't only for storing data. To reduce overhead, it's recommended to copy or move the objects instead of creating new ones (for example by copying a static object from a factory method).

### SerialisableAny - serialise simple types effortlessly

Some struct types are simple enough to be considered heterogeneous arrays rather than classes. There's no way to learn the member names, so they have to be identified only as indexes of an array.

For this to work, the class must be aggregate-initialisable and have no parent class. Simplified rules for aggregate-initialisability are:
* No private members
* No written constructor
* No virtual member functions

Methods and default member values are allowed.

```C++
struct Unsupported {
	int a = 3;
	std::string b = "I am not gay.";
	float c = 4.5;
	bool d = true;
	std::shared_ptr<std::string> e = nullptr;
	short int f = 13;
	double g = 14.34;
	struct {
		int a;
		double b;
	} h;
};

std::string source = "[15, \"High albedo, low roughness\", 17.424, false, null, 18, 123.214, [814, 241.134]]";
Unsupported made = readJsonObject<Unsupported>(source);
std::cout << "Member test: " << made.b << std::endl;
Serialisable::JSON remade = writeJsonObject(made);
std::cout << "Reserialised: " << remade << std::endl;
```
If C++17 is not available, member objects cannot have default values and must contain only primitive types and objects that are already allowed to be member objects.

### A more condensed format

This is a binary markup language designed to use as little space as possible while keeping the same expressive power than JSON (and is thus directly convertible to JSON and back). It's also significantly more space efficient than BSON, Packed JSON. It's also more space efficient than MessagePack. However, it's slow to write. Its only purpose is to take as little space as possible while keeping the versatility of JSON. It's not a compression algorithm, so under some circumstances, it might be compressed afterwards to further reduce the size if (for example if it contains a lot of strings).

You need to include `condensed_json.hpp` for it to work. It can be accessed using methods `to<CondensedJSON>()` and `from<CondensedJSON()` or `saveAs<CondensedJSON>()` and `loadAs<CondensedJSON>()`, using data saved in a `std::vector<uint8_t>`:

```C++
// Assuming 'value' inherits from Serialisable
std::vector<uint8_t> data = value.to<CondensedJSON>();
//...
value.loadAs<CondensedJSON>();
```

In most cases, it uses one byte of markup per value, only long strings and some composite objects need two bytes. Small integers and boolean values are included in the markup bytes and take no additional space themselves. ASCII-only element names in objects are only as long as the strings themselves and the element names are stored only once if there are more objects with the same element names.

#### Encoding

It has more types than JSON, but they are selected automatically for better space efficiency and translate to the same JSON. The types are marked with binary prefixes, while the prefixes may contain data themselves:
* **1xxxxxxx** - forms a 15 bit float with the next byte (almost half-precision), 1 bit is sign, 6 are exponent, 8 are mantissa, so the imprecision is about 0.2% and maximal value is in the order of ten power 9 *(total size is 2)*
* **011xxxxx** - string of size below 30, size is stored in the remaining bits *(total size is length + 1)*
* **01111110** - reserved
* **01111111** - long string, zero-terminated *(total size is length + 2)*
* **010xxxxx** - 5 bit signed integer, saved in the type *(total size is 1)*
* **00111xxx** - object whose member names must contain ASCII-symbols and objects with the same layout appear more than once in the JSON, first occurrence comes with a zero-terminated definition of all member names terminated by the most significant bit flipped, last three bits form the identifier *(total size of object is contents + 1 and once element names + 1)*
* **00111101** - reserved
* **00111110** - same, but identifier is the following byte *(total size of object is contents + 2 and once element names + 1)*
* **00111111** - same, but the identifier are the two following bytes *(total size of object is contents + 3 and once element names + 1)*
* **00110xxx** - small object with layout appearing only once, with max size up to 5 written in the type, with ASCII-only element names first and contents after *(total size is contents + element names + 1)*
* **00110110** - large object whose zero-terminated names list must contain ASCII-symbols only *(total size is of object is contents + element names + 2)*
* **00110111** - large, zero-terminated hashtable/object whose zero terminated member names do not contain only ASCII-symbols *(total size is contents + element names + number of elements + 2)*
* **0010xxxx** - array of size up to 14, size is a part of the type, size is stored in the type *(total size 1 + total size of contents)*
* **00101110** - reserved
* **00101111** - large, zero-terminated array of objects *(total size is 2 + total size of contents)*
* **0001xxxx** - forms a 12 bit signed integer with the following byte *(size is 2)*
* **00001111** - double (written in little endian, least significant bit goes first) *(size is 9)*
* **00001110** - float *(size is 5)*
* **00001101** - signed 64 bit integer *(size is 9)*
* **00001100** - unsigned 64 bit integer *(size is 9)*
* **00001011** - signed 32 bit integer *(size is 5)*
* **00001010** - unsigned 32 bit integer *(size is 5)*
* **00001001** - signed 16 bit integer *(size is 3)*
* **00001000** - unsigned 16 bit integer *(size is 3)*
* **000001xx** - reserved
* **00000011** - true *(size is 1)*
* **00000010** - false *(size is 1)*
* **00000001** - null *(size is 1)*
* **00000000** - zero termination, for terminating strings, objects/hashtables and arrays too large to have their sizes in the type byte *(size is counted to types that need it)*

### SerialisablePolymorphic - different classes in one field
If a field can contain various types of objects, it can be dealt with by keeping it in the JSON format and dealing with it later, but `serialisable_polymorphic.hpp` makes it more convenient. It uses [GenericFactory](https://github.com/Dugy/generic_factory), a small, header-only library providing a generic implementation of the self-registering factory pattern. You will have to add it to your project in order to use this tool.

It allows descendants of a certain parent class that inherits from `SerialisablePolymorphic` to be selected according to a specific key that identifies the type (`_pt`). The parent class can be `SerialisablePolymorphic` itself. Every subclass registers itself as a descendant with some name of the parent class. Then the right subclass can be selected when loading the JSON.

The following example allows the members of the class `Master` to contain the two subclasses of `ContentType` and be serialised and deserialised intact:

```C++
struct ContentType : public SerialisablePolymorphic {
	bool fullscreen = false;
	void serialisation() override {
		synch("fullscreen", fullscreen);
	}
};

struct Content1 : public ContentType {
	std::string value;
	void serialisation() override {
		ContentType::serialisation();
		subclass("c1");
		synch("value", value);
	}
	// Registering variant 1 (more readable)
	SERIALISABLE_REGISTER_POLYMORPHIC(ContentType, Content1, "c1");
};

struct Content2 : public ContentType {
	double value;
	void serialisation() override {
		ContentType::serialisation();
		subclass("c2");
		synch("value", value);
	}
	// Registering variant 2 (no macros)
	inline const static polymorphism<ContentType, Content2> = "c2";
};

struct Master : public Serialisable {
	std::vector<std::shared_ptr<ContentType>> contents;
	std::unique_ptr<ContentType> main;
	void serialisation() override {
		synch("contents", contents);
		synch("main", main);
	}
};
```

If you don't have C++17, it's not possible to use `inline static` variables, so you will have to resort to the way provided by `GenericFactory`:

```C++
REGISTER_CHILD_INTO_FACTORY(ContentType, Content1, "c1");
// Must be in the same namespace as the class, but not inside the class
```

### Better enums
Before `reflexpr` is finished and its support is added to all major compilers (probably in C++23), there's no standard-compliant way to determine the human-readable value of an enum without some kind of dictionary. Because `reflexpr` would make most of this library useless, it's better not to wait for it.

The [Better Enums](https://github.com/aantron/better-enums) library implements a macro that can create an enum-like structure that can be conveniently converted into string and back and plenty of other handy utilities.

If you have it in your project, include `serialisable_better_enum.hpp` to be able to serialise better enums as well.

Note: better enums are identified using duck typing, so there is a small chance that it would be mistaken for another class with very similar external interface.

## Extending it yourself
The functionality can be extended to some extent without editing the original files.

### Custom types

It is possible to extend the library to serialise custom types and containers as well. You can do this by specialising `SerialisableInternals::Serialiser` with your type. For example, if you want to serialise also `std::map` indexed with `std::string`, include this file instead of `serialisable.hpp`.

```C++
#include <map>
#include "serialisable.hpp"

namespace SerialisableInternals {

template <typename T>
struct Serialiser<std::map<std::string, T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true; // Show that it's not unspecialised
	static_assert (Serialiser<T, void>::valid, "Serialising a map of non-serialisable types");

	static std::shared_ptr<Serialisable::JSONobject> serialise(const std::map<std::string, T>& value) {
		auto made = std::make_shared<Serialisable::JSONobject>();
		for (auto& it : value)
			made->_contents[it.first] = Serialiser<T, void>::serialise(it.second);
		return made;
	}

	static void deserialise(std::map<std::string, T>& result, std::shared_ptr<Serialisable::JSON> value) {
		const std::map<std::string, std::shared_ptr<Serialisable::JSON>>& got = value->getObject();
		for (auto it = result.begin(); it != result.end(); ) {
			if (got.find(it->first) == got.end())
				it = result.erase(it);
			else
				++it;
		}
		for (auto& it : got)
			Serialiser<T, void>::deserialise(result[it.first], it.second);
	}
};

}
```

To implement a custom serialisation for a custom class that does inherit from `Serialisable`, have it inherit from `SerialisableInternals::UnusualSerialisable` as well, which will stop the default serialisation functions from being selected.

### Custom file types

Suport for custom types can be added by implementing a class that can serialise and deserialise the internal JSON representation into data. The class must contain:
* A static method called `serialise` that accepts the JSON and returns its representation in an iterable container (`std::string` or `std::vector<uint8_t>` is fine)
* A static method called `deserialise` that accepts the representation of the type returned by `serialise` and returns the internal JSON representation

For a special implementation for writing into files, you can use:
* A static method named `toStream` that accepts the internal JSON representation, a reference to an ostream and any number of parameters with default values (usable in recursion)
* A static method named `fromStream` that returns the internal JSON representation, and accepts a reference to an istream and any number of parameters with default values (usable in recursion)

Otherwise, the first two methods will be used.

So if the class is named `PDF`, then you can use it to convert into the format using the `to<PDF>()` and `from<PDF>()` methods and to save them to files using the `saveAs<PDF>()` and `loadAs<PDF>()` methods (both on `Serialisable` and `Serialisable::JSON`).
