/// A microlisp named Wisp, by Adam McDaniel

////////////////////////////////////////////////////////////////////////////////
/// LANGUAGE OPTIONS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Comment this define out to drop support for libm functions
#define HAS_LIBM
#ifdef HAS_LIBM
#include <cmath>
#else
#define NO_LIBM_SUPPORT "no libm support"
#endif


// Comment this define out to drop support for standard library functions.
// This allows the program to run without a runtime.
#define USE_STD
#ifdef USE_STD
#include <cstdlib>
#include <iostream>
#include <fstream>

std::string read_file_contents(std::string filename) {
    std::ifstream f;
    f.open(filename.c_str());
    if (!f)
        throw std::runtime_error("could not open file");

    f.seekg(0, std::ios::end);
    std::string contents;
    contents.reserve(f.tellg());
    f.seekg(0, std::ios::beg);
    contents.assign(std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>());
    f.close();

    return contents;
}
#else
#define NO_STD "no standard library support"
#endif


////////////////////////////////////////////////////////////////////////////////
/// REQUIRED INCLUDES //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <exception>

////////////////////////////////////////////////////////////////////////////////
/// ERROR MESSAGES /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define TOO_FEW_ARGS "too few arguments to function"
#define TOO_MANY_ARGS "too many arguments to function"
#define INVALID_ARGUMENT "invalid argument"
#define MISMATCHED_TYPES "mismatched types"
#define CALL_NON_FUNCTION "called non-function"
#define UNKNOWN_ERROR "unknown exception"
#define INVALID_LAMBDA "invalid lambda"
#define INVALID_BIN_OP "invalid binary operation"
#define INVALID_ORDER "cannot order expression"
#define BAD_CAST "cannot cast"
#define ATOM_NOT_DEFINED "atom not defined"
#define EVAL_EMPTY_LIST "evaluated empty list"
#define INTERNAL_ERROR "interal virtual machine error"
#define MALFORMED_PROGRAM "malformed program"

////////////////////////////////////////////////////////////////////////////////
/// TYPE NAMES /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define STRING_TYPE "string"
#define INT_TYPE "int"
#define FLOAT_TYPE "float"
#define UNIT_TYPE "unit"
#define FUNCTION_TYPE "function"
#define ATOM_TYPE "atom"
#define QUOTE_TYPE "quote"
#define LIST_TYPE "list"

////////////////////////////////////////////////////////////////////////////////
/// HELPER FUNCTIONS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Convert an object to a string using a stringstream conveniently
#define to_string( x ) static_cast<std::ostringstream&>((std::ostringstream() << std::dec << x )).str()

// Is this character a valid lisp symbol character
bool is_symbol(char ch) {
    return (isalpha(ch) || ispunct(ch)) && ch != '(' && ch != ')' && ch != '"' && ch != '\'';
}

////////////////////////////////////////////////////////////////////////////////
/// LISP CONSTRUCTS ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Forward declaration for Environment class definition
class Value;


// An instance of a function's scope.
class Environment {
public:
    // Default constructor
    Environment() : parent_scope(NULL) {}
    // Default constructor
    Environment(Environment *parent) : parent_scope(parent) {}

    // Get the value associated with this name in this scope
    Value get(std::string name);
    // Set the value associated with this name in this scope
    void set(std::string name, Value value);

    void combine(Environment const &other);

    void set_parent_scope(Environment *parent) {
        parent_scope = parent;
    }
    
    // Output this scope in readable form to a stream.
    friend std::ostream &operator<<(std::ostream &os, Environment const &v);
private:

    // The definitions in the scope.
    std::map<std::string, Value> defs;
    Environment *parent_scope;
};


// An exception thrown by the lisp
class Error {
public:
    // Create an error with the value that caused the error,
    // the scope where the error was found, and the message.
    Error(Value v, Environment env, const char *msg);
    ~Error();

    // Get the printable error description.
    std::string description();
private:
    Value *cause;
    Environment env;
    const char *msg;
};

// The type for a builtin function, which takes a list of values,
// and the environment to run the function in.
typedef Value (*Builtin)(std::vector<Value>, Environment &);

class Value {
public:
    ////////////////////////////////////////////////////////////////////////////////
    /// CONSTRUCTORS ///////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Constructs a unit value
    inline Value() : type(UNIT) {}

    // Constructs an integer
    inline Value(int i) : type(INT) { stack_data.i = i; }
    // Constructs a floating point value
    inline Value(double f) : type(FLOAT) { stack_data.f = f; }
    // Constructs a list
    inline Value(std::vector<Value> list) : type(LIST), list(list) {}

    // Quote a value
    static Value quote(Value quoted) {
        Value result;
        result.type = QUOTE;

        // The first position in the list is
        // used to store the quoted expression.
        result.list.push_back(quoted);
        return result;
    }

    // Construct an atom
    static Value atom(std::string s) {
        Value result;
        result.type = ATOM;

        // We use the `str` member to store the atom.
        result.str = s;
        return result;
    }

    // Construct a string
    static Value string(std::string s) {
        Value result;
        result.type = STRING;

        // We use the `str` member to store the string.
        result.str = s;
        return result;
    }


    // Construct a lambda function
    Value(std::vector<Value> params, Value ret, Environment env) : type(LAMBDA) {
        // We store the params and the result in the list member
        // instead of having dedicated members. This is to save memory.
        list.push_back(Value(params));
        list.push_back(ret);

        // The environment has its own member.
        lambda_scope = env;
    }

    // Construct a builtin function
    inline Value(std::string name, Builtin b) : type(BUILTIN) {
        // Store the name of the builtin function in the str member
        // to save memory, and use the builtin function slot in the union
        // to store the function pointer.
        str = name;
        stack_data.b = b;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// C++ INTEROP METHODS ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Is this a builtin function?
    inline bool is_builtin() {
        return type == BUILTIN;
    }

    // Apply this as a function to a list of arguments in a given environment.
    Value apply(std::vector<Value> args, Environment &env);
    // Evaluate this value as lisp code.
    Value eval(Environment &env);

    bool is_number() const {
        return type == INT || type == FLOAT;
    }

    // Get the "truthy" boolean value of this value.
    inline bool as_bool() const {
        return *this != Value(0);
    }

    // Get this item's integer value
    inline int as_int() const {
        return cast_to_int().stack_data.i;
    }

    // Get this item's floating point value
    inline double as_float() const {
        return cast_to_int().stack_data.f;
    }

    // Get this item's string value
    inline std::string as_string() const {
        // If this item is not a string, throw a cast error.
        if (type != STRING)
            throw Error(*this, Environment(), BAD_CAST);
        return str;
    }

    // Get this item's atom value
    inline std::string as_atom() const {
        // If this item is not an atom, throw a cast error.
        if (type != ATOM)
            throw Error(*this, Environment(), BAD_CAST);
        return str;
    }

    // Get this item's list value
    inline std::vector<Value> as_list() const {
        // If this item is not a list, throw a cast error.
        if (type != LIST)
            throw Error(*this, Environment(), BAD_CAST);
        return list;
    }

    // Push an item to the end of this list
    void push(Value val) {
        // If this item is not a list, you cannot push to it.
        // Throw an error.
        if (type != LIST)
            throw Error(*this, Environment(), MISMATCHED_TYPES);
        
        list.push_back(val);
    }

    // Push an item from the end of this list
    Value pop() {
        // If this item is not a list, you cannot pop from it.
        // Throw an error.
        if (type != LIST)
            throw Error(*this, Environment(), MISMATCHED_TYPES);
        
        // Remember the last item in the list
        Value result = list[list.size()-1];
        // Remove it from this instance
        list.pop_back();
        // Return the remembered value
        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// TYPECASTING METHODS ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Cast this to an integer value
    Value cast_to_int() const {
        switch (type) {
        case INT: return *this;
        case FLOAT: return Value(int(stack_data.f));
        // Only ints and floats can be cast to an int
        default:
            throw Error(*this, Environment(), BAD_CAST);
        }
    }

    // Cast this to a floating point value
    Value cast_to_float() const {
        switch (type) {
        case FLOAT: return *this;
        case INT: return Value(float(stack_data.i));
        // Only ints and floats can be cast to a float
        default:
            throw Error(*this, Environment(), BAD_CAST);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// COMPARISON OPERATIONS //////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    bool operator==(Value other) const {
        // If either of these values are floats, promote the
        // other to a float, and then compare for equality.
        if (type == FLOAT && other.type == INT) return *this == other.cast_to_float();
        else if (type == INT && other.type == FLOAT) return this->cast_to_float() == other;
        // If the values types aren't equal, then they cannot be equal.
        else if (type != other.type) return false;

        switch (type) {
        case FLOAT:
            return stack_data.f == other.stack_data.f;
        case INT:
            return stack_data.i == other.stack_data.i;
        case BUILTIN:
            return stack_data.b == other.stack_data.b;
        case STRING:
        case ATOM:
            // Both atoms and strings store their
            // data in the str member.
            return str == other.str;
        case LAMBDA:
        case LIST:
            // Both lambdas and lists store their
            // data in the list member.
            return list == other.list;
        case QUOTE:
            // The values for quotes are stored in the
            // first slot of the list member.
            return list[0] == other.list[0];
        default:
            return true;
        }
    }
    
    inline bool operator!=(Value other) const {
        return !(*this == other);
    }

    // bool operator<(Value other) const {
    //     if (other.type != FLOAT && other.type != INT)
    //         throw Error(*this, Environment(), INVALID_BIN_OP);

    //     switch (type) {
    //     case FLOAT:
    //         return stack_data.f < other.cast_to_float().stack_data.f;
    //     case INT:
    //         if (other.type == FLOAT)
    //             return cast_to_float().stack_data.f < other.stack_data.f;
    //         else return stack_data.i < other.stack_data.i;
    //     default:
    //         throw Error(*this, Environment(), INVALID_ORDER);
    //     }
    // }
    
    ////////////////////////////////////////////////////////////////////////////////
    /// ORDERING OPERATIONS ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    inline bool operator>=(Value other) const {
        return !(*this < other);
    }
    
    inline bool operator<=(Value other) const {
        return (*this == other) || (*this < other);
    }
    
    inline bool operator>(Value other) const {
        return !(*this <= other);
    }

    bool operator<(Value other) const {
        // Other type must be a float or an int
        if (other.type != FLOAT && other.type != INT)
            throw Error(*this, Environment(), INVALID_BIN_OP);

        switch (type) {
        case FLOAT:
            // If this is a float, promote the other value to a float and compare.
            return stack_data.f < other.cast_to_float().stack_data.f;
        case INT:
            // If the other value is a float, promote this value to a float and compare.
            if (other.type == FLOAT)
                return cast_to_float().stack_data.f < other.stack_data.f;
            // Otherwise compare the integer values
            else return stack_data.i < other.stack_data.i;
        default:
            // Only allow comparisons between integers and floats
            throw Error(*this, Environment(), INVALID_ORDER);
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    /// ARITHMETIC OPERATIONS //////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // This function adds two lisp values, and returns the lisp value result.
    Value operator+(Value other) const {
        // If the other value's type is the unit type,
        // don't even bother continuing.
        // Unit types consume all arithmetic operations.
        if (other.type == UNIT) return other;

        // Other type must be a float or an int
        if ((is_number() || other.is_number()) &&
            !(is_number() && other.is_number()))
            throw Error(*this, Environment(), INVALID_BIN_OP);

        switch (type) {
        case FLOAT:
            // If one is a float, promote the other by default and do
            // float addition.
            return Value(stack_data.f + other.cast_to_float().stack_data.f);
        case INT:
            // If the other type is a float, go ahead and promote this expression
            // before continuing with the addition.
            if (other.type == FLOAT)
                return Value(cast_to_float() + other.stack_data.f);
            // Otherwise, do integer addition.
            else return Value(stack_data.i + other.stack_data.i);
        case STRING:
            // If the other value is also a string, do the concat
            if (other.type == STRING)
                return Value::string(str + other.str);
            // We throw an error if we try to concat anything of non-string type
            else throw Error(*this, Environment(), INVALID_BIN_OP);
        case LIST:
            // If the other value is also a list, do the concat
            if (other.type == LIST) {
                // Maintain the value that will be returned
                Value result = *this;
                // Add each item in the other list to the end of this list
                for (size_t i=0; i<other.list.size(); i++)
                    result.push(other.list[i]);
                return result;
            
            } else throw Error(*this, Environment(), INVALID_BIN_OP);
        case UNIT:
            return *this;
        default:
            throw Error(*this, Environment(), INVALID_BIN_OP);
        }
    }

    // This function subtracts two lisp values, and returns the lisp value result.
    Value operator-(Value other) const {
        // If the other value's type is the unit type,
        // don't even bother continuing.
        // Unit types consume all arithmetic operations.
        if (other.type == UNIT) return other;

        // Other type must be a float or an int
        if (other.type != FLOAT && other.type != INT)
            throw Error(*this, Environment(), INVALID_BIN_OP);

        switch (type) {
        case FLOAT:
            // If one is a float, promote the other by default and do
            // float subtraction.
            return Value(stack_data.f - other.cast_to_float().stack_data.f);
        case INT:
            // If the other type is a float, go ahead and promote this expression
            // before continuing with the subtraction
            if (other.type == FLOAT)
                return Value(cast_to_float().stack_data.f - other.stack_data.f);
            // Otherwise, do integer subtraction.
            else return Value(stack_data.i - other.stack_data.i);
        case UNIT:
            // Unit types consume all arithmetic operations.
            return *this;
        default:
            // This operation was done on an unsupported type
            throw Error(*this, Environment(), INVALID_BIN_OP);
        }
    }

    // This function multiplies two lisp values, and returns the lisp value result.
    Value operator*(Value other) const {
        // If the other value's type is the unit type,
        // don't even bother continuing.
        // Unit types consume all arithmetic operations.
        if (other.type == UNIT) return other;

        // Other type must be a float or an int
        if (other.type != FLOAT && other.type != INT)
            throw Error(*this, Environment(), INVALID_BIN_OP);
        
        switch (type) {
        case FLOAT:
            return Value(stack_data.f * other.cast_to_float().stack_data.f);
        case INT:
            // If the other type is a float, go ahead and promote this expression
            // before continuing with the product
            if (other.type == FLOAT)
                return Value(cast_to_float().stack_data.f * other.stack_data.f);
            // Otherwise, do integer multiplication.
            else return Value(stack_data.i * other.stack_data.i);
        case UNIT:
            // Unit types consume all arithmetic operations.
            return *this;
        default:
            // This operation was done on an unsupported type
            throw Error(*this, Environment(), INVALID_BIN_OP);
        }
    }

    // This function divides two lisp values, and returns the lisp value result.
    Value operator/(Value other) const {
        // If the other value's type is the unit type,
        // don't even bother continuing.
        // Unit types consume all arithmetic operations.
        if (other.type == UNIT) return other;

        // Other type must be a float or an int
        if (other.type != FLOAT && other.type != INT)
            throw Error(*this, Environment(), INVALID_BIN_OP);

        switch (type) {
        case FLOAT:
            return Value(stack_data.f / other.cast_to_float().stack_data.f);
        case INT:
            // If the other type is a float, go ahead and promote this expression
            // before continuing with the product
            if (other.type == FLOAT)
                return Value(cast_to_float().stack_data.f / other.stack_data.f);
            // Otherwise, do integer multiplication.
            else return Value(stack_data.i / other.stack_data.i);
        case UNIT:
            // Unit types consume all arithmetic operations.
            return *this;
        default:
            // This operation was done on an unsupported type
            throw Error(*this, Environment(), INVALID_BIN_OP);
        }
    }

    // This function finds the remainder of two lisp values, and returns the lisp value result.
    Value operator%(Value other) const {
        // If the other value's type is the unit type,
        // don't even bother continuing.
        // Unit types consume all arithmetic operations.
        if (other.type == UNIT) return other;

        // Other type must be a float or an int
        if (other.type != FLOAT && other.type != INT)
            throw Error(*this, Environment(), INVALID_BIN_OP);
        
        switch (type) {
        // If we support libm, we can find the remainder of floating point values.
        #ifdef HAS_LIBM
        case FLOAT:
            return Value(fmod(stack_data.f, other.cast_to_float().stack_data.f));
        case INT:
            if (other.type == FLOAT)
                return Value(fmod(cast_to_float().stack_data.f, other.stack_data.f));
            else return Value(stack_data.i % other.stack_data.i);

        #else
        case INT:
            // If we do not support libm, we have to throw errors for floating point values.
            if (other.type != INT)
                throw Error(other, Environment(), NO_LIBM_SUPPORT);
            return Value(stack_data.i % other.stack_data.i);
        #endif

        case UNIT:
            // Unit types consume all arithmetic operations.
            return *this;
        default:
            // This operation was done on an unsupported type
            throw Error(*this, Environment(), INVALID_BIN_OP);
        }
    }

    // Get the name of the type of this value
    std::string get_type_name() {
        switch (type) {
        case QUOTE: return QUOTE_TYPE;
        case ATOM: return ATOM_TYPE;
        case INT: return INT_TYPE;
        case FLOAT: return FLOAT_TYPE;
        case LIST: return LIST_TYPE;
        case STRING: return STRING_TYPE;
        case BUILTIN:
        case LAMBDA:
            // Instead of differentiating between
            // lambda and builtin types, we group them together.
            // This is because they are both callable.
            return FUNCTION_TYPE;
        case UNIT:
            return UNIT_TYPE;
        default:
            // We don't know the name of this type.
            // This isn't the users fault, this is just unhandled.
            // This should never be reached.
            throw Error(*this, Environment(), INTERNAL_ERROR);
        }
    }

    std::string display() const {
        std::string result;
        switch (type) {
        case QUOTE:
            return "'" + list[0].debug();
        case ATOM:
            return str;
        case INT:
            return to_string(stack_data.i);
        case FLOAT:
            return to_string(stack_data.f);
        case STRING:
            return str;
        case LAMBDA:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(lambda " + result + ")";
        case LIST:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(" + result + ")";
        case BUILTIN:
            return "<" + str + " at " + to_string(long(stack_data.b)) + ">";
        case UNIT:
            return "@";
        default:
            // We don't know how to display whatever type this is.
            // This isn't the users fault, this is just unhandled.
            // This should never be reached.
            throw Error(*this, Environment(), INTERNAL_ERROR);
        }
    }

    std::string debug() const {
        std::string result;
        switch (type) {
        case QUOTE:
            return "'" + list[0].debug();
        case ATOM:
            return str;
        case INT:
            return to_string(stack_data.i);
        case FLOAT:
            return to_string(stack_data.f);
        case STRING:
            for (size_t i=0; i<str.length(); i++) {
                if (str[i] == '"') result += "\\\"";
                else result.push_back(str[i]);
            }
            return "\"" + result + "\"";
        case LAMBDA:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(lambda " + result + ")";
        case LIST:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(" + result + ")";
        case BUILTIN:
            return "<" + str + " at " + to_string(long(stack_data.b)) + ">";
        case UNIT:
            return "@";
        default:
            // We don't know how to debug whatever type this is.
            // This isn't the users fault, this is just unhandled.
            // This should never be reached.
            throw Error(*this, Environment(), INTERNAL_ERROR);
        }
    }

    inline friend std::ostream &operator<<(std::ostream &os, Value const &v) {
        return os << v.display();
    }

private:
    enum {
        QUOTE,
        ATOM,
        INT,
        FLOAT,
        LIST,
        STRING,
        LAMBDA,
        BUILTIN,
        UNIT
    } type;

    union {
        int i;
        double f;
        Builtin b;
    } stack_data;

    std::string str;
    std::vector<Value> list;
    Environment lambda_scope;
};

inline Error::Error(Value v, Environment env, const char *msg) : env(env), msg(msg) {
    cause = new Value;
    *cause = v;
}

inline Error::~Error() {
    delete cause;
}

inline std::string Error::description() {
    return "error: the expression `" + cause->debug() + "` failed in scope " + to_string(env) + " with message \"" + msg + "\"";
}


void Environment::combine(Environment const &other) {
    defs.insert(other.defs.begin(), other.defs.end());
}

std::ostream &operator<<(std::ostream &os, Environment const &e) {
    std::map<std::string, Value>::const_iterator itr = e.defs.begin();
    os << "{ ";
    for (; itr != e.defs.end(); itr++) {
        os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
    }
    return os << "}";
}

inline void Environment::set(std::string name, Value value) {
    defs[name] = value;
}


Value Value::apply(std::vector<Value> args, Environment &env) {
    Environment e;
    std::vector<Value> params;
    switch (type) {
    case LAMBDA:
        params = list[0].list;
        if (params.size() > args.size())
            throw Error(Value(args), env, TOO_FEW_ARGS);
        else if (params.size() < args.size())
            throw Error(Value(args), env, TOO_MANY_ARGS);

        e = lambda_scope;
        e.set_parent_scope(&env);

        for (size_t i=0; i<params.size(); i++) {
            if (params[i].type != ATOM) 
                throw Error(*this, env, INVALID_LAMBDA);
            e.set(params[i].str, args[i]);
        }

        return list[1].eval(e);
    case BUILTIN:
        return (stack_data.b)(args, env);
    default:
        throw Error(*this, env, CALL_NON_FUNCTION);
    }
}


Value Value::eval(Environment &env) {
    std::vector<Value> args;
    Value function;
    Environment e;
    switch (type) {
    case QUOTE:
        return list[0];
    case ATOM:
        return env.get(str);
    case LIST:
        if (list.size() < 1)
            throw Error(*this, env, EVAL_EMPTY_LIST);

        args = std::vector<Value>(list.begin() + 1, list.end());
        
        // Only evaluate our arguments if it's not builtin!
        // Builtin functions can be special forms, so we
        // leave them to evaluate their arguments.
        function = list[0].eval(env);

        if (!function.is_builtin())
            for (size_t i=0; i<args.size(); i++)
                args[i] = args[i].eval(env);

        return function.apply(
            args,
            env
        );

    default:
        return *this;
    }
}

inline void skip_whitespace(std::string &s, int &ptr) {
    while (isspace(s[ptr])) { ptr++; }
}

Value parse(std::string &s, int &ptr) {
    skip_whitespace(s, ptr);

    if (s[ptr] == ';') {
        // If this is a comment
        int save_ptr = ptr;
        while (s[save_ptr] != '\n' && save_ptr < int(s.length())) { save_ptr++; }
        s.erase(ptr, save_ptr - ptr);
        skip_whitespace(s, ptr);

        if (s.substr(ptr, s.length()-ptr-1) == "")
            return Value();
    }


    if (s == "") {
        return Value();
    } else if (s[ptr] == '\'') {
        // If this is a quote
        ptr++;
        return Value::quote(parse(s, ptr));

    } else if (s[ptr] == '(') {
        // If this is a list
        skip_whitespace(s, ++ptr);

        Value result = Value(std::vector<Value>());

        while (s[ptr] != ')')
            result.push(parse(s, ptr));
        
        skip_whitespace(s, ++ptr);
        return result;
        
    } else if (isdigit(s[ptr]) || (s[ptr] == '-' && isdigit(s[ptr + 1]))) {
        // If this is a number
        bool negate = s[ptr] == '-';
        if (negate) ptr++;
        
        int save_ptr = ptr;
        while (isdigit(s[ptr]) || s[ptr] == '.') ptr++;
        std::string n = s.substr(save_ptr, ptr);
        skip_whitespace(s, ptr);
        
        if (n.find('.') != std::string::npos)
            return Value((negate? -1 : 1) * atof(n.c_str()));
        else return Value((negate? -1 : 1) * atoi(n.c_str()));

    } else if (s[ptr] == '\"') {
        // If this is a string
        int n = 1;
        while (s[ptr + n] != '\"') {
            if (ptr + n >= int(s.length()))
                throw std::runtime_error(MALFORMED_PROGRAM);
                
            if (s[ptr + n] == '\\') n++;
            n++;
        }

        std::string x = s.substr(ptr+1, n-1);
        ptr += n+1;
        skip_whitespace(s, ptr);
        while (x.find("\\\"") != std::string::npos)
            x.replace(x.find("\\\""), 2, "\"");
        return Value::string(x);
    } else if (s[ptr] == '@') {
        ptr++;
        skip_whitespace(s, ptr);
        return Value();

    } else if (is_symbol(s[ptr])) {
        // If this is a string
        int n = 0;
        while (is_symbol(s[ptr + n])) {
            n++;
        }

        std::string x = s.substr(ptr, n);
        ptr += n;
        skip_whitespace(s, ptr);
        return Value::atom(x);
    } else {
        throw std::runtime_error(MALFORMED_PROGRAM);
    }
}

std::vector<Value> parse(std::string s) {
    int i=0, last_i=-1;
    std::vector<Value> result;
    while (last_i != i && i <= int(s.length()-1)) {
        last_i = i;
        result.push_back(parse(s, i));
    }

    if (i < int(s.length()))
        throw std::runtime_error(MALFORMED_PROGRAM);

    return result;
}

Value run(std::string code, Environment &env) {
    std::vector<Value> parsed = parse(code);
    for (size_t i=0; i<parsed.size()-1; i++)
        parsed[i].eval(env);

    return parsed[parsed.size()-1].eval(env);
}

namespace builtin {
    inline void eval_args(std::vector<Value> &args, Environment &env) {
        for (size_t i=0; i<args.size(); i++)
            args[i] = args[i].eval(env);
    }

    Value lambda(std::vector<Value> args, Environment &env) {
        if (args.size() < 2)
            throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

        if (args[0].get_type_name() != LIST_TYPE)
            throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

        return Value(args[0].as_list(), args[1], env);
    }

    Value if_then_else(std::vector<Value> args, Environment &env) {
        if (args.size() != 3)
            throw Error(Value("if", if_then_else), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
        if (args[0].eval(env).as_bool())
            return args[1].eval(env);
        else return args[2].eval(env);
    }

    Value define(std::vector<Value> args, Environment &env) {
        if (args.size() != 2)
            throw Error(Value("define", define), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            
        Value result = args[1].eval(env);
        env.set(args[0].display(), result);
        return result;
    }

    Value defun(std::vector<Value> args, Environment &env) {
        if (args.size() != 3)
            throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

        if (args[1].get_type_name() != LIST_TYPE)
            throw Error(Value("defun", defun), env, INVALID_LAMBDA);

        Value f = Value(args[1].as_list(), args[2], env);
        env.set(args[0].display(), f);
        return f;
    }

    Value do_block(std::vector<Value> args, Environment &env) {
        Value acc;
        for (size_t i=0; i<args.size(); i++)
            acc = args[i].eval(env);
        return acc;
    }

    Value scope(std::vector<Value> args, Environment &env) {
        Environment e = env;
        Value acc;
        for (size_t i=0; i<args.size(); i++)
            acc = args[i].eval(e);
        return acc;
    }

    Value quote(std::vector<Value> args, Environment &env) {
        std::vector<Value> v;
        for (size_t i=0; i<args.size(); i++)
            v.push_back(args[i]);
        return Value(v);
    }

    #ifdef USE_STD
    Value exit(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::exit(args.size() < 1? 0 : args[0].cast_to_int().as_int());
        return Value();
    }

    Value print(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() < 1)
            throw Error(Value("print", print), env, TOO_FEW_ARGS);

        Value acc;
        for (size_t i=0; i<args.size(); i++) {
            acc = args[i];
            std::cout << acc.display();
            if (i < args.size() - 1)
                std::cout << " ";
        }
        std::cout << std::endl;
        return acc;
    }

    Value input(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() > 1)
            throw Error(Value("input", input), env, TOO_MANY_ARGS);

        if (!args.empty())
            std::cout << args[0];

        std::string s;
        std::getline(std::cin, s);
        return Value::string(s);
    }

    Value read_file(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("read-file", read_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        // return Value::string(content);
        return Value::string(read_file_contents(args[0].as_string()));
    }

    Value write_file(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("write-file", write_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::ofstream f;
        f.open(args[0].as_string().c_str());
        Value result = Value((f << args[1].as_string())? 1 : 0);
        f.close();
        return result;
    }

    Value include(std::vector<Value> args, Environment &env) {
        // Import is technically not a special form, it's more of a macro.
        // We can evaluate our arguments.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("include", include), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        Environment e;
        Value result = run(read_file_contents(args[0].as_string()), e);
        env.combine(e);
        return result;
    }
    #endif

    inline Value eval(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        else return args[0].eval(env);
    }

    inline Value list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);
        
        return Value(args);
    }

    Value sum(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);
        
        if (args.size() < 2)
            throw Error(Value("+", sum), env, TOO_FEW_ARGS);
        
        Value acc = args[0];
        for (size_t i=1; i<args.size(); i++)
            acc = acc + args[i];
        return acc;
    }

    inline Value subtract(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] - args[1];
    }

    Value product(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() < 2)
            throw Error(Value("*", product), env, TOO_FEW_ARGS);

        Value acc = args[0];
        for (size_t i=1; i<args.size(); i++)
            acc = acc * args[i];
        return acc;
    }

    inline Value divide(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] / args[1];
    }

    inline Value remainder(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] % args[1];
    }

    inline Value eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] == args[1]));
    }

    inline Value neq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] != args[1]));
    }

    inline Value greater(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] > args[1]));
    }

    inline Value less(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] < args[1]));
    }

    inline Value greater_eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] >= args[1]));
    }

    inline Value less_eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] <= args[1]));
    }

    Value get_type_name(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].get_type_name());
    }


    Value cast_to_float(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].cast_to_float();
    }

    Value cast_to_int(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].cast_to_int();
    }

    Value push(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() == 0)
            throw Error(Value("push", push), env, TOO_FEW_ARGS);
        for (size_t i=1; i<args.size(); i++)
            args[0].push(args[i]);
        return args[0];
    }

    Value pop(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].pop();
    }

    Value head(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].as_list()[0];
    }

    Value tail(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::vector<Value> v;
        for (size_t i=1; i<args[0].as_list().size(); i++) {
            v.push_back(args[0].as_list()[i]);
        }
        return Value(v);
    }

    Value parse(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        if (args[0].get_type_name() != STRING_TYPE)
            throw Error(args[0], env, INVALID_ARGUMENT);
        std::vector<Value> parsed = ::parse(args[0].as_string());

        // if (parsed.size() == 1)
        //     return parsed[0];
        // else return Value(parsed);
        return Value(parsed);
    }

    Value display(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].display());
    }

    Value debug(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].debug());
    }

    Value map_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> result, l=args[1].as_list(), tmp;
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(l[i]);
            result.push_back(args[0].apply(tmp, env));
            tmp.clear();
        }
        return Value(result);
    }

    Value filter_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> result, l=args[1].as_list(), tmp;
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(l[i]);
            if (args[0].apply(tmp, env).as_bool())
                result.push_back(l[i]);
            tmp.clear();
        }
        return Value(result);
    }

    Value reduce_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> l=args[2].as_list(), tmp;
        Value acc = args[1];
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(acc);
            tmp.push_back(l[i]);
            acc = args[0].apply(tmp, env);
            tmp.clear();
        }
        return acc;
    }

    Value range(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> result;
        Value low = args[0], high = args[1];
        if (low.get_type_name() != INT_TYPE && low.get_type_name() != FLOAT_TYPE)
            throw Error(low, env, MISMATCHED_TYPES);
        if (high.get_type_name() != INT_TYPE && high.get_type_name() != FLOAT_TYPE)
            throw Error(high, env, MISMATCHED_TYPES);

        if (low >= high) return Value(result);

        while (low < high) {
            result.push_back(low);
            low = low + Value(1);
        }
        return Value(result);
    }
}

void repl(Environment &env) {
#ifdef USE_STD
    std::string code;
    std::string input;
    Value tmp;
    std::vector<Value> parsed;
    while (true) {
        std::cout << ">>> ";
        std::getline(std::cin, input);
        if (input == "!quit" || input == "!q")
            break;
        else if (input == "!env" || input == "!e")
            std::cout << env << std::endl;
        else if (input == "!export" || input == "!x") {
            std::cout << "File to export to: ";
            std::getline(std::cin, input);

            std::ofstream f;
            f.open(input.c_str(), std::ofstream::out);
            f << code;
            f.close();
        } else if (input != "") {
            try {
                tmp = run(input, env);
                std::cout << " => " << tmp.debug() << std::endl;
                code += input + "\n";
            } catch (Error &e) {
                std::cerr << e.description() << std::endl;
            } catch (std::runtime_error &e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }
#endif
}


Value Environment::get(std::string name) {
    // Meta operations
    if (name == "eval")  return Value("eval",  builtin::eval);
    if (name == "type")  return Value("type",  builtin::get_type_name);
    if (name == "parse") return Value("parse", builtin::parse);

    // Special forms
    if (name == "do")    return Value("do",    builtin::do_block);
    if (name == "if")    return Value("if",    builtin::if_then_else);
    if (name == "scope") return Value("scope", builtin::scope);
    if (name == "quote") return Value("quote", builtin::quote);
    if (name == "defun") return Value("defun", builtin::defun);
    if (name == "define") return Value("define", builtin::define);
    if (name == "lambda") return Value("lambda", builtin::lambda);

    // Comparison operations
    if (name == "=")  return Value("=",  builtin::eq);
    if (name == "!=") return Value("!=", builtin::neq);
    if (name == ">")  return Value(">",  builtin::greater);
    if (name == "<")  return Value("<",  builtin::less);
    if (name == ">=") return Value(">=", builtin::greater_eq);
    if (name == "<=") return Value("<=", builtin::less_eq);

    // Arithmetic operations
    if (name == "+") return Value("+", builtin::sum);
    if (name == "-") return Value("-", builtin::subtract);
    if (name == "*") return Value("*", builtin::product);
    if (name == "/") return Value("/", builtin::divide);
    if (name == "%") return Value("%", builtin::remainder);

    // List operations
    if (name == "list")  return Value("list",  builtin::list);
    if (name == "push")  return Value("push",  builtin::push);
    if (name == "pop")   return Value("pop",   builtin::pop);
    if (name == "head")  return Value("head",  builtin::head);
    if (name == "tail")  return Value("tail",  builtin::tail);
    if (name == "first") return Value("first", builtin::head);
    if (name == "last")  return Value("last",  builtin::pop);
    if (name == "range") return Value("range",  builtin::range);

    // Functional operations
    if (name == "map")    return Value("map",    builtin::map_list);
    if (name == "filter") return Value("filter", builtin::filter_list);
    if (name == "reduce") return Value("reduce", builtin::reduce_list);

    // IO operations
    #ifdef USE_STD
    if (name == "exit")   return Value("exit",  builtin::exit);
    if (name == "quit")   return Value("quit",  builtin::exit);
    if (name == "print")  return Value("print", builtin::print);
    if (name == "input")  return Value("input", builtin::input);
    if (name == "include") return Value("include", builtin::include);
    if (name == "read-file")  return Value("read-file",  builtin::read_file);
    if (name == "write-file") return Value("write-file", builtin::write_file);
    #endif

    // Formatting operations
    if (name == "debug")   return Value("debug",   builtin::debug);
    if (name == "display") return Value("display", builtin::display);
    
    // Casting operations
    if (name == "int")   return Value("int",   builtin::cast_to_int);
    if (name == "float") return Value("float", builtin::cast_to_float);

    // Constants
    if (name == "endl") return Value::string("\n");
    
    std::map<std::string, Value>::iterator itr = defs.find(name);
    if (itr != defs.end()) return itr->second;
    else if (parent_scope != NULL) {
        itr = parent_scope->defs.find(name);
        if (itr != parent_scope->defs.end()) return itr->second;
        else return parent_scope->get(name);
    }

    throw Error(Value::atom(name), *this, ATOM_NOT_DEFINED);
}

int main(int argc, const char **argv) {
    Environment env;

    try {
        if (argc == 1 || (argc == 2 && std::string(argv[1]) == "-i"))
            repl(env);
        else if (argc == 3 && std::string(argv[1]) == "-f")
            run(read_file_contents(argv[2]), env);
        else if (argc == 3 && std::string(argv[1]) == "-c")
            run(argv[2], env);
        #ifdef USE_STD
        else std::cerr << "invalid arguments" << std::endl;
        #endif
    } catch (Error &e) {
        std::cerr << e.description() << std::endl;
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}