#ifndef H_OBJECTS_H
#define H_OBJECTS_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <memory>

#include "ast/ast.hpp"
#include "code/code.hpp"

namespace objects
{
	enum class ObjectType
	{
		Null,
		ERROR,
		INTEGER,
		BOOLEAN,
		STRING,
		RETURN_VALUE,
		FUNCTION,
		ARRAY,
		HASH,
		BUILTIN,
		COMPILED_FUNCTION,
		CLOSURE,
	};

	struct HashKey
	{
		ObjectType Type;
		uint64_t Value;

		HashKey(ObjectType type, uint64_t val) : Type(type), Value(val) {}

		bool operator==(const HashKey &rhs) const
		{
			return (Type == rhs.Type && Value == rhs.Value);
		}

		bool operator!=(const HashKey &rhs) const
		{
			return (Type != rhs.Type || Value != rhs.Value);
		}

		bool operator<(const HashKey &rhs) const
		{
			return (Value < rhs.Value);
		}
	};

	struct Object
	{
		virtual ~Object() {}
		virtual ObjectType Type() { return ObjectType::Null; }
		virtual bool Hashable(){ return false; }
		virtual std::string Inspect() { return ""; }

		virtual HashKey GetHashKey() {
			return HashKey(Type(), 0);
		}

		std::string TypeStr()
		{
			switch (Type())
			{
			case ObjectType::Null:
				return "Null";
			case ObjectType::ERROR:
				return "ERROR";
			case ObjectType::INTEGER:
				return "INTEGER";
			case ObjectType::BOOLEAN:
				return "BOOLEAN";
			case ObjectType::STRING:
				return "STRING";
			case ObjectType::RETURN_VALUE:
				return "RETURN_VALUE";
			case ObjectType::FUNCTION:
				return "FUNCTION";
			case ObjectType::ARRAY:
				return "ARRAY";
			case ObjectType::HASH:
				return "HASH";
			case ObjectType::BUILTIN:
				return "BUILTIN";
			case ObjectType::COMPILED_FUNCTION:
				return "COMPILED_FUNCTION";
			default:
				return "BadType";
			}
		}
	};

	struct Integer : Object
	{
		long long int Value;

		Integer() {}
		Integer(long long int val) : Value(val) {}

		virtual ~Integer() {}
		virtual ObjectType Type() { return ObjectType::INTEGER; }
		virtual bool Hashable(){ return true; }
		virtual std::string Inspect()
		{
			std::stringstream oss;
			oss << Value;
			return oss.str();
		}

		virtual HashKey GetHashKey() {
			return HashKey(Type(), static_cast<uint64_t>(Value));
		}
	};

	struct Boolean : Object
	{
		bool Value;

		Boolean(bool val) : Value(val) {}

		virtual ~Boolean() {}
		virtual ObjectType Type() { return ObjectType::BOOLEAN; }
		virtual bool Hashable(){ return true; }
		virtual std::string Inspect()
		{
			std::stringstream oss;
			oss << (Value ? "true" : "false");
			return oss.str();
		}

		virtual HashKey GetHashKey() {
			if (Value)
			{
				return HashKey(Type(), 1);
			}
			else
			{
				return HashKey(Type(), 0);
			}
		}
	};

	struct String : Object
	{
		std::string Value;

		String(): Value(""){}
		String(std::string val) : Value(val) {}

		virtual ~String() {}
		virtual ObjectType Type() { return ObjectType::STRING; }
		virtual bool Hashable(){ return true; }
		virtual std::string Inspect()
		{
			return "\"" + Value + "\"";
		}

		virtual HashKey GetHashKey() {
			auto intVal = std::hash<std::string>{}(Value);
			return HashKey(Type(), static_cast<uint64_t>(intVal));
		}
	};

	struct Array : Object
	{
		std::vector<std::shared_ptr<Object>> Elements;

		Array(){}
		Array(std::vector<std::shared_ptr<Object>>& elements): Elements(elements){}
		virtual ~Array() {}
		virtual ObjectType Type() { return ObjectType::ARRAY; }
		virtual std::string Inspect()
		{
			std::stringstream oss;
			std::vector<std::string> items{};
			for (auto &item : Elements)
			{
				items.push_back(item->Inspect());
			}
			oss << "[" << ast::Join(items, ", ") << "]";
			return oss.str();
		}
	};

	struct Null : Object
	{
		Null() {}
		virtual ~Null() {}
		virtual ObjectType Type() { return ObjectType::Null; }
		virtual std::string Inspect() { return "null"; }
	};

	struct ReturnValue : Object
	{
		std::shared_ptr<Object> Value;

		ReturnValue(std::shared_ptr<Object> val) : Value(val) {}
		virtual ~ReturnValue() { Value.reset();}
		virtual ObjectType Type() { return ObjectType::RETURN_VALUE; }
		virtual std::string Inspect() { return Value->Inspect(); }
	};

	struct Error : Object
	{
		std::string Message;

		virtual ~Error() {}
		virtual ObjectType Type() { return ObjectType::ERROR; }
		virtual std::string Inspect() { return "ERROR: " + Message; }
	};

	struct HashPair
	{
		std::shared_ptr<Object> Key;
		std::shared_ptr<Object> Value;

		HashPair(std::shared_ptr<Object> key, std::shared_ptr<Object> val): Key(key), Value(val){}
	};

	struct Hash: Object
	{
		std::map<HashKey, std::shared_ptr<HashPair>> Pairs;

		Hash(std::map<HashKey, std::shared_ptr<HashPair>>& pairs): Pairs(pairs){}
		virtual ~Hash(){}
		virtual ObjectType Type() { return ObjectType::HASH; }
		virtual std::string Inspect() 
		{ 
			std::stringstream oss;
			std::vector<std::string> items{};
			for (auto &[key, pair] : Pairs)
			{
				[[maybe_unused]] auto x = key;
				items.push_back(pair->Key->Inspect() + ": " + pair->Value->Inspect());
			}
			oss << "{" << ast::Join(items, ", ") << "}";
			return oss.str(); 
		}
	};

	struct Environment;

	struct Function : Object
	{
		std::vector<std::shared_ptr<ast::Identifier>> Parameters;
		std::shared_ptr<ast::BlockStatement> Body;
		std::shared_ptr<Environment> Env;

		virtual ~Function() {
			Parameters.clear();
			Body.reset();
			Env.reset();
		}
		virtual ObjectType Type() { return ObjectType::FUNCTION; }
		virtual std::string Inspect()
		{
			std::stringstream oss;

			std::vector<std::string> params{};

			for (auto &p : Parameters)
			{
				params.push_back(p->String());
			}

			oss << "fn(" << ast::Join(params, ", ") << ") {\n"
				<< Body->String() << "\n}";
			return oss.str();
		}
	};

	struct CompiledFunction: Object
	{
		bytecode::Instructions Instructions;
		int NumLocals;
		int NumParameters;

		CompiledFunction(bytecode::Instructions &ins, const int &numLocals, const int &numParameters)
			: Instructions(ins),
			  NumLocals(numLocals),
			  NumParameters(numParameters)
		{
		}

		virtual ObjectType Type() { return ObjectType::COMPILED_FUNCTION; }
		virtual std::string Inspect()
		{
			std::stringstream oss;
			oss << "CompiledFunction[" << this << "]";
			return oss.str();
		}
	};

	struct Closure: Object
	{
		std::shared_ptr<CompiledFunction> Fn;
		std::vector<std::shared_ptr<Object>> Free;

		Closure(std::shared_ptr<CompiledFunction> fn): Fn(fn){}
		Closure(std::shared_ptr<CompiledFunction> fn, std::vector<std::shared_ptr<Object>> free): Fn(fn), Free(free){}
		virtual ~Closure(){}

		virtual ObjectType Type() { return ObjectType::CLOSURE; }
		virtual std::string Inspect()
		{
			std::stringstream oss;
			oss << "Closure[" << this << "]";
			return oss.str();
		}
	};

	static std::shared_ptr<objects::Null> NULL_OBJ = std::make_shared<objects::Null>();
	static std::shared_ptr<objects::Boolean> TRUE_OBJ = std::make_shared<objects::Boolean>(true);
	static std::shared_ptr<objects::Boolean> FALSE_OBJ = std::make_shared<objects::Boolean>(false);

	std::shared_ptr<objects::Error> newError(std::string msg)
	{
		std::shared_ptr<objects::Error> error = std::make_shared<objects::Error>();
		error->Message = msg;
		return error;
	}

	bool isError(std::shared_ptr<objects::Object> obj)
	{
		if (obj != nullptr)
		{
			return (obj->Type() == objects::ObjectType::ERROR);
		}
		return false;
	}

	bool isTruthy(std::shared_ptr<objects::Object> obj)
	{
		if (obj == objects::NULL_OBJ)
		{
			return false;
		}
		else if (obj == objects::TRUE_OBJ)
		{
			return true;
		}
		else if (obj == objects::FALSE_OBJ)
		{
			return false;
		}
		else
		{
			return true;
		}
	}


	std::shared_ptr<objects::Boolean> nativeBoolToBooleanObject(bool input)
	{
		if (input)
		{
			return objects::TRUE_OBJ;
		}
		else
		{
			return objects::FALSE_OBJ;
		}
	}

	std::shared_ptr<objects::Object> evalArrayIndexExpression(std::shared_ptr<objects::Object> left, std::shared_ptr<objects::Object> index)
	{
		std::shared_ptr<objects::Array> arrayObj = std::dynamic_pointer_cast<objects::Array>(left);
		auto idx = std::dynamic_pointer_cast<objects::Integer>(index)->Value;
		auto max = static_cast<int64_t>(arrayObj->Elements.size() - 1);

		if(idx < 0 || idx > max)
		{
			return objects::NULL_OBJ;
		}

		return arrayObj->Elements[idx];
	}

	std::shared_ptr<objects::Object> evalHashIndexExpression(std::shared_ptr<objects::Object> left, std::shared_ptr<objects::Object> index)
	{
		std::shared_ptr<objects::Hash> hashObj = std::dynamic_pointer_cast<objects::Hash>(left);

		if(!index->Hashable())
		{
			return objects::newError("unusable as hash key: " + index->TypeStr());
		}

		auto hashed = index->GetHashKey();

		auto fit = hashObj->Pairs.find(hashed);
		if(fit == hashObj->Pairs.end())
		{
			return objects::NULL_OBJ;
		}

		return fit->second->Value;
	}


}

#endif // H_OBJECTS_H