/*
 * Simple JSON parse/serialize class (needs C++11).
 * Implementation details:
 *   - Use COW (Copy on Write) technique.
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXJSON_HPP
#define EXJSON_HPP

#include <initializer_list>
#include <vector>
#include <map>
#include <memory>
#include "math.h"
#include "string.h"

//#define exjson_debug(fmt, args...) printf(fmt, ## args)
#define exjson_debug(fmt, args...)

namespace ExJSON {

class ExJSONVal;
class ExJSONData;

typedef std::shared_ptr<ExJSONData> ExJSONDataPtr;
typedef std::vector<ExJSONVal> ExJSONValVec;
typedef std::map<std::string, ExJSONVal> ExJSONValMap;

typedef enum {
	ExJSONValNull,
	ExJSONValInt,
	ExJSONValBool,
	ExJSONValDouble,
	ExJSONValString,
	ExJSONValArray,
	ExJSONValObject
} ExJSONValType;

union ExJSONDataUnion {
	ExJSONValVec *v;
	ExJSONValMap *m;
	std::string  *s;
	double        d;
	long          i;
	bool          b;
};


class ExJSONData {
public:
	ExJSONData() {
		m_type = ExJSONValNull;
		m_u.v = NULL;
		exjson_debug("ExJSONData constructor %d\n", this);
	}
	~ExJSONData() {
		exjson_debug("ExJSONData destructor %d\n", this);
		clear();
	}

	/*!
	 * \brief Allocate memory for a given type.
	 * \param t - Variant type.
	 */
	void alloc(ExJSONValType t) {
		if (m_type == t) return; /* Do not alloc on the same type */
		clear();    /* Clear if value is set         */
		m_type = t;
		switch (m_type) {
			case ExJSONValArray:  {m_u.v = new ExJSONValVec(); exjson_debug("Allocate memory for Array %d\n", this); } break;
			case ExJSONValObject: {m_u.m = new ExJSONValMap(); exjson_debug("Allocate memory for Object %d\n", this);} break;
			case ExJSONValString: {m_u.s = new std::string(); exjson_debug("Allocate memory for String %d\n", this);} break;
			/* Do not need to allocate anything */
			default: { m_u.v = NULL; } break;
		}
	}

	/*!
	 * \brief Allocate and copy.
	 * \param t - Other data pointer.
	 */
	void copy(const ExJSONDataPtr &t) {
		clear();    /* Clear if value is set    */
		m_type = t->m_type;
		switch (m_type) {
			case ExJSONValArray: {
				m_u.v = new ExJSONValVec(*(t->m_u.v));
				exjson_debug("Copy array %d, %d\n", t, this);
			} break;
			case ExJSONValObject: {
				m_u.m = new ExJSONValMap(*(t->m_u.m));
				exjson_debug("Copy object %d, %d\n", t, this);
			} break;
			case ExJSONValString: {
				m_u.s = new std::string(*(t->m_u.s));
				exjson_debug("Copy string %d, %d\n", t, this);
			} break;
			case ExJSONValInt: {
				m_u.i = t->m_u.i;
				exjson_debug("Copy int %d, %d\n", t, this);
			} break;
			case ExJSONValBool: {
				m_u.b = t->m_u.b;
				exjson_debug("Copy bool %d, %d\n", t, this);
			} break;
			case ExJSONValDouble: {
				m_u.d = t->m_u.d;
				exjson_debug("Copy double %d, %d\n", t, this);
			} break;
			case ExJSONValNull: break;
		}
	}

	/*!
	 * \brief Cleanup memory.
	 */
	void clear() {
		if (m_u.v) {
			if (m_type == ExJSONValArray) {
				exjson_debug("Cleanup memory: Array\n");
				delete m_u.v;
			} else if (m_type == ExJSONValObject) {
				exjson_debug("Cleanup memory: Object\n");
				delete m_u.m;
			} else if (m_type == ExJSONValString) {
				exjson_debug("Cleanup memory: String\n");
				delete m_u.s;
			} else {
				/* Do not need to clean anything */
			}
		}
		/* Set as NULL */
		m_type = ExJSONValNull;
		m_u.v = NULL;
	}

	/* Int */
	void setInt(const long &i) {alloc(ExJSONValInt); m_u.i = i; }
	long getInt() const {
		if ((m_type == ExJSONValInt) || (m_type == ExJSONValBool)) return m_u.i;   /* Native value        */
		else if (m_type == ExJSONValDouble) return ::round(m_u.d);                 /* Round to integer    */
		else if (m_type == ExJSONValString) return std::stol(*m_u.s);              /* Try to parse string */
		return 0;
	}

	/* Bool */
	void setBool(const bool &i) {alloc(ExJSONValBool); m_u.b = i; }
	bool getBool() const {
		if ((m_type == ExJSONValBool) || (m_type == ExJSONValInt)) return (m_u.b);
		return false;
	}

	/* Double */
	void setDouble(const double &i) {alloc(ExJSONValDouble); m_u.d = i; }
	double getDouble() const {
		if (m_type == ExJSONValDouble) return m_u.d;                    /* Native value          */
		else if (m_type == ExJSONValInt) return (double)(m_u.i);        /* Convert int to double */
		else if (m_type == ExJSONValString) return std::stod(*m_u.s);   /* Try to parse string   */
		return 0.0;
	}

	/* String */
	void setString(const std::string &s) {
		alloc(ExJSONValString);
		*m_u.s = s;
	}

	/* CString */
	void setCString(const char *&s, int &len) {
		if (len < 0) len = strlen(s);
		alloc(ExJSONValString);
		*m_u.s = std::string(s, len);
	}

	/* Array */
	void setArray(const ExJSONValVec &s) {
		clear();
		m_type = ExJSONValArray;
		m_u.v = new ExJSONValVec(s);
		exjson_debug("Alloc Array - copy %d\n", this);
	}

	/* Object */
	void setObject(ExJSONValMap &s) {
		clear();
		m_type = ExJSONValObject;
		m_u.m = new ExJSONValMap(s);
	}

public:
	ExJSONValType   m_type;
	ExJSONDataUnion m_u;
};



class ExJSONVal {
public:
	/*!
	 * \brief Constructors.
	 */
	ExJSONVal()                    { d = std::make_shared<ExJSONData>();                     }
	ExJSONVal(ExJSONValType t)     { d = std::make_shared<ExJSONData>(); d->alloc(t);        }
	ExJSONVal(const ExJSONVal &t)  { exjson_debug("Clone pointer (constructor)\n"); d = t.d; }
	ExJSONVal(unsigned int v)      { d = std::make_shared<ExJSONData>(); d->setInt((long)v); }
	ExJSONVal(int v)               { d = std::make_shared<ExJSONData>(); d->setInt((long)v); }
	ExJSONVal(long v)              { d = std::make_shared<ExJSONData>(); d->setInt(v);       }
	ExJSONVal(bool v)              { d = std::make_shared<ExJSONData>(); d->setBool(v);      }
	ExJSONVal(double v)            { d = std::make_shared<ExJSONData>(); d->setDouble(v);    }
	ExJSONVal(const std::string &s){ d = std::make_shared<ExJSONData>(); d->setString(s);    }
	ExJSONVal(ExJSONValVec &s)     { d = std::make_shared<ExJSONData>(); d->setArray(s);     }
	ExJSONVal(ExJSONValMap &s)     { d = std::make_shared<ExJSONData>(); d->setObject(s);    }
	ExJSONVal(const char *s, int len = -1) {
		d = std::make_shared<ExJSONData>();
		d->setCString(s, len);
	}
	ExJSONVal(std::initializer_list<ExJSONVal> l) {
		d = std::make_shared<ExJSONData>();
		auto i = l.begin();
		if (l.size() == 1) {
			/* This is single element */
			d = i->d;
			return;
		}
		/* Detect object or array */
		if (checkForObject(l)) {
			d->alloc(ExJSONValObject);
			ExJSONValMap *v = d->m_u.m;
			while (i != l.end()) {
				std::string key = i->getString(); i++;
				(*v)[key] = ExJSONVal((*i++));
			}
		} else {
			d->alloc(ExJSONValArray);
			ExJSONValVec *v = d->m_u.v;
			while (i != l.end()) {
				v->push_back(ExJSONVal((*i++)));
			}
		}
	}
	/* Copy operator  */
	ExJSONVal &operator = ( const ExJSONVal &t ) { d = t.d; return *this; }

	/*!
	 * \brief Destructor.
	 */
	~ExJSONVal() { }

	//Detach this string from the original and create a buffer for its own.
	void _detach(bool copy = false) {
		if (d.unique()) return;
		if (copy) {
			exjson_debug("Detach and copy\n");
			ExJSONDataPtr old = d;
			d.reset( new ExJSONData() ); //Be careful with uniform initialization and narrowing conversions here
			d->copy(old);
		} else {
			exjson_debug("Detach\n");
			d.reset( new ExJSONData() ); //Be careful with uniform initialization and narrowing conversions here
		}
	}


	/*!
	 * \brief getType - get JSON variant type.
	 */
	ExJSONValType getType() const { return d->m_type; }
	bool is_null() const   { return (d->m_type == ExJSONValNull);   }
	bool is_double() const { return (d->m_type == ExJSONValDouble); }
	bool is_bool() const   { return (d->m_type == ExJSONValBool);   }
	bool is_int() const    { return (d->m_type == ExJSONValInt);    }
	bool is_string() const { return (d->m_type == ExJSONValString); }
	bool is_object() const { return (d->m_type == ExJSONValObject); }
	bool is_array() const  { return (d->m_type == ExJSONValArray);  }

	/* ============--- Integer ---============== */
	ExJSONVal &operator = ( const long &i ) { _detach(); d->setInt(i); return *this; }
	ExJSONVal &operator = ( const int &i ) { _detach(); d->setInt((long)i); return *this; }
	ExJSONVal &operator = ( const unsigned int &i ) { _detach(); d->setInt((long)i); return *this; }
	long getInt() const { return d->getInt(); }
	long to_int() const { return d->getInt(); }

	/* ============--- Bool ---============== */
	ExJSONVal &operator = ( const bool &i ) { _detach(); d->setBool(i); return *this; }
	bool getBool() const { return d->getBool(); }
	bool to_bool() const { return d->getBool(); }

	/* ============--- double ---============== */
	ExJSONVal &operator = ( const double &i ) { _detach(); d->setDouble(i); return *this; }
	double getDouble() const { return d->getDouble(); }
	double to_double() const { return d->getDouble(); }

	/* ============--- string ---============== */
	ExJSONVal &operator = ( const char *i ) { _detach(); d->setString(i); return *this; }
	ExJSONVal &operator = ( const std::string &s ) { _detach(); d->setString(s); return *this; }
	std::string getString() const {
		if (d->m_type == ExJSONValString) return *(d->m_u.s);                                     /* Get native value.          */
		else if (d->m_type == ExJSONValDouble) return std::to_string(d->m_u.d);                   /* Convert double to string.  */
		else if (d->m_type == ExJSONValInt) return std::to_string(d->m_u.i);                      /* Convert int to string.     */
		else if (d->m_type == ExJSONValBool) return std::string((d->m_u.b)?"true":"false");       /* Convert bool to string.    */
		else if ((d->m_type == ExJSONValArray) || (d->m_type == ExJSONValObject)) return dump();  /* Dump to json string.       */
		return std::string();
	}
	std::string to_string() const { return getString(); }

	/* ============--- Array ---============== */
	void push_back(long v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back int %d\n", v);
	}
	void push_back(int v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back int %d\n", v);
	}
	void push_back(double v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back double %f\n", v);
	}
	void push_back(const char *v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back c string %d, %s\n", d, v);
	}
	void push_back(const std::string &v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back string %d %s\n", d, v.c_str());
	}
	void push_back(ExJSONVal p) {
		_detach(true);
		d->alloc(ExJSONValArray);
		ExJSONValVec *v = d->m_u.v;
		v->push_back(std::move(p));
	}
	ExJSONVal& operator[](int i) {
		_detach(true);
		d->alloc(ExJSONValArray);
		ExJSONValVec *v =  d->m_u.v;
		/* Resize vector if needed */
		while (((int)v->size()) < (i + 1)) v->push_back(ExJSONVal());
		exjson_debug("Array set %d, idx = %d\n", d, i);
		return ((*v)[i]);
	}
	ExJSONValVec getList() const {
		if (d->m_type == ExJSONValArray) return *d->m_u.v;
		return ExJSONValVec();
	}
	ExJSONValVec to_list() const { return getList(); }

	ExJSONValVec *getListPtr() {
		if (d->m_type == ExJSONValArray) return d->m_u.v;
		return NULL;
	}
//	ExJSONValVec& getListRef() {
//		if (d->m_type == ExJSONValArray) return (*d->m_u.v);
//		_detach(true);
//		d->alloc(ExJSONValArray);
//		return (*d->m_u.v);
//	}

	ExJSONVal at(int i) {
		if (d->m_type != ExJSONValArray) return ExJSONVal();
		ExJSONValVec *v =  d->m_u.v;
		return ((*v)[i]);
	}

	/* ============--- Object ---============== */
	/* Check for object */
	bool checkForObject(std::initializer_list<ExJSONVal> &l) {
		if (l.size()%2) return false;
		auto i = l.begin();
		while (i != l.end()) {
			if (i->getType() != ExJSONValString) return false;
			i++;
			i++;
		}
		return true;
	}

	ExJSONVal& operator[](const char *i) {
		_detach(true);
		d->alloc(ExJSONValObject);
		ExJSONValMap *v =  d->m_u.m;
		auto x = v->find(i);
		if (x == v->end()) {
			// printf("Not found %s\n", i);
			v->insert({i, ExJSONVal()});
			x = v->find(i);
		}
		return (x->second);
	}
	void setKey(std::string i, ExJSONVal y) {
		_detach(true);
		d->alloc(ExJSONValObject);
		ExJSONValMap *v = d->m_u.m;
		auto x = v->find(i);
		if (x == v->end()) {
			v->insert({i, y});
		}
	}

	ExJSONVal getKey(std::string s) {
		if (d->m_type == ExJSONValObject) {
			ExJSONValMap *v =  d->m_u.m;
			auto x = v->find(s);
			if (x == v->end()) {
				return ExJSONVal();
			}
			return (x->second);
		}
		return ExJSONVal();
	}

	bool contains(std::string s) {
		if (d->m_type == ExJSONValObject) {
			ExJSONValMap *v =  d->m_u.m;
			auto x = v->find(s);
			if (x == v->end()) {
				return false;
			}
			return true;
		} else if (d->m_type == ExJSONValArray) {
		}
		return false;
	}

	std::string dump() const{ std::string res; res.reserve(128); dumpInt(res); return res; }

	/* Simplified operators */
	bool operator == (const ExJSONVal& b) const {
//		if ((d->m_type == ExJSONValArray) && (b.getType() == ExJSONValArray)) {
//			/* Smart compare ? */
//		}
		return (getString() == b.getString());
	}


	/* --- Parser (based on https://github.com/nbsdx/SimpleJSON/blob/master/json.hpp ) --- */

	static inline void consume_ws(const char *&str) { while( isspace( *str ) ) ++str; }

	static ExJSONVal parse_object(const char *&str) {
		ExJSONVal obj( ExJSONValObject );

		++str;
		consume_ws(str);
		if( *str == '}' ) {
			++str;
			return obj;
		}

		while( true ) {
			ExJSONVal key = parse_next(str);
			consume_ws(str);
			if (*str != ':') break;
			consume_ws(++str);
			obj.setKey(key.getString(), parse_next(str));
			consume_ws(str);
			if( *str == ',' ) {
				++str;
				continue;
			} else if( *str == '}' ) {
				++str;
				break;
			} else {
				break;
			}
		}
		return obj;
	}

	static ExJSONVal parse_array(const char *&str) {
		ExJSONVal arr( ExJSONValArray );

		++str;
		consume_ws(str);
		if (*str == ']') {
			++str;
			return arr;
		}

		while( true ) {
			ExJSONVal v = parse_next(str);
			arr.push_back(v);
			consume_ws(str);
			if( *str == ',' ) {
				++str;
				continue;
			} else if (*str == ']') {
				++str;
				break;
			} else {
				return arr;
			}
		}
		return arr;
	}

	static ExJSONVal parse_string(const char *&str) {
		const char *o = ++str;
		while ((*str != '\"') && (*str != '\0')) str++;
		str++;
		return ExJSONVal(o, str - o - 1);
	}

	static ExJSONVal parse_number(const char *&str) {
		ExJSONVal n;
		std::string val, exp_str;
		char c;
		bool isDouble = false;
		long exp = 0;
		while( true ) {
			c = *str++;
			if( (c == '-') || (c >= '0' && c <= '9') )
				val += c;
			else if( c == '.' ) {
				val += c;
				isDouble = true;
			} else
				break;
		}
		if( c == 'E' || c == 'e' ) {
			c = *str++;
			if( c == '-' ){ ++str; exp_str += '-';}
			while( true ) {
				c = *str++;
				if( c >= '0' && c <= '9' )
					exp_str += c;
				else if( !isspace( c ) && c != ',' && c != ']' && c != '}' ) {
					return n;
				} else
					break;
			}
			exp = std::stol( exp_str );
		} else if( !isspace( c ) && c != ',' && c != ']' && c != '}' ) {
			return n;
		}
		--str;
		if( isDouble )
			n = std::stod( val ) * std::pow( 10, exp );
		else {
			if( !exp_str.empty() )
				n = std::stol( val ) * std::pow( 10, exp );
			else
				n = std::stol( val );
		}
		return n;
	}

	static ExJSONVal parse_bool(const char *&str) {
		ExJSONVal b;
		if (!strncmp( str, "true", 4 )) {
			b = true;
			str += 4;
		} else if(!strncmp( str, "false", 5 )) {
			b = false;
			str += 5;
		} else {
			return b;
		}
		return b;
	}

	static ExJSONVal parse_null(const char *&str) {
		ExJSONVal n;
		if (strncmp( str, "null", 4 )) {
			return n;
		}
		str += 4;
		return n;
	}

	static ExJSONVal parse_next( const char *&str ) {
		char value;
		consume_ws(str);
		value = *str;
		switch( value ) {
			case '[' : return parse_array(str);
			case '{' : return parse_object(str);
			case '\"': return parse_string(str);
			case 't' :
			case 'f' : return parse_bool(str);
			case 'n' : return parse_null(str);
			default  : if( ( value <= '9' && value >= '0' ) || value == '-' )
					return parse_number(str);
		}
		return 	ExJSONVal();
	}

	static ExJSONVal parse(const std::string s) { return parse(s.c_str()); }
	static ExJSONVal parse(const char *str) { return parse_next(str); }

	/*!
	 * \brief Force parse as array.
	 */
	static ExJSONVal fromArray(std::initializer_list<ExJSONVal> l) {
		ExJSONVal v(ExJSONValArray);
		auto i = l.begin();
		while (i != l.end()) {
			ExJSONVal x((*i++));
			v.push_back(x);
		}
		return v;
	}


private:
	/*!
	 * \brief Construct JSON string.
	 * \return JSON string.
	 */
	void dumpInt(std::string &s) const {
		switch(d->m_type) {
			case ExJSONValNull:   s.append("null"); break;
			case ExJSONValInt:    s.append(std::to_string(d->m_u.i)); break;
			case ExJSONValBool:   s.append((d->m_u.b)?"true":"false"); break;
			case ExJSONValDouble: s.append(std::to_string(d->m_u.d)); break;
			case ExJSONValString: s.append("\"").append(getString()).append("\""); break;
			case ExJSONValArray: {
				s.append("[");
				ExJSONValVec *v = d->m_u.v;
				for (const auto &i: *v) {
					i.dumpInt(s);
					s.append(",");
				}
				if (v->size() > 0) s.resize(s.size() - 1);
				s.append("]");
			} break;
			case ExJSONValObject: {
				s.append("{");
				ExJSONValMap *v = d->m_u.m;
				for (const auto &i: *v) {
					s.append("\"").append(i.first).append("\":");
					i.second.dumpInt(s);
					s.append(",");
				}
				if (v->size() > 0) s.resize(s.size() - 1);
				s.append("}");
			} break;
			default: break;
		}
	}

private:
	ExJSONDataPtr d;
};

}

#endif // EXJSON_HPP
