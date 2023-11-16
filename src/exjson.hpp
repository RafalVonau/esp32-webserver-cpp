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

class ExJSONData {
public:
	ExJSONData() {
		m_type = ExJSONValNull;
		m_len = 0;
		m_value = NULL;
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
		if (m_value) clear();    /* Clear if value is set         */
		m_type = t;
		switch (m_type) {
			case ExJSONValArray:  {m_value=new ExJSONValVec(); exjson_debug("Allocate memory for Array %d\n", m_value); } break;
			case ExJSONValObject: {m_value=new ExJSONValMap(); exjson_debug("Allocate memory for Object %d\n", m_value);} break;
			case ExJSONValDouble: {m_len=sizeof(double); m_value = malloc(m_len); exjson_debug("Allocate memory for double %d\n", m_value); } break;
			case ExJSONValString: {
				m_len = 1;
				m_value = malloc(m_len);
				((char *)m_value)[0] = '\0';
				exjson_debug("Allocate memory for string %d\n", m_value);
			} break;
				/* Do not need to allocate anything */
			default: { m_value = NULL; m_len   = 0; } break;
		}
	}

	/*!
	 * \brief Allocate and copy.
	 * \param t - Other data pointer.
	 */
	void copy(const ExJSONDataPtr &t) {
		clear();    /* Clear if value is set    */
		m_type = t->m_type;
		m_len  = t->m_len;
		switch (m_type) {
			case ExJSONValArray: {
				ExJSONValVec *v = (ExJSONValVec *)t->m_value;
				m_value = new ExJSONValVec(*v);
				exjson_debug("Copy array %d, %d\n", t->m_value, m_value);
			} break;
			case ExJSONValObject: {
				ExJSONValMap *v = (ExJSONValMap *)t->m_value;
				m_value = new ExJSONValMap(*v);
				exjson_debug("Copy object %d, %d\n", t->m_value, m_value);
			} break;
			case ExJSONValInt:
			case ExJSONValBool: {
				m_value = NULL;
				exjson_debug("Copy int/bool %d, %d\n", t->m_len, m_len);
			} break;
			default: {
				/* COPY string/double */
				m_value = malloc(m_len);
				memcpy(m_value, t->m_value, m_len);
				exjson_debug("Copy string/double %d, %d\n", t->m_value, m_value);
			} break;
		}
	}

	/*!
	 * \brief Cleanup memory.
	 */
	void clear() {
		if (m_value) {
			if (m_type == ExJSONValArray) {
				exjson_debug("Cleanup memory: Array\n");
				ExJSONValVec *v = (ExJSONValVec *)m_value;
				delete v;
			} else if (m_type == ExJSONValObject) {
				exjson_debug("Cleanup memory: Object\n");
				ExJSONValMap *v = (ExJSONValMap *)m_value;
				delete v;
			} else if ((m_type == ExJSONValInt) || (m_type == ExJSONValBool)) {
				/* Do not need to clean anything */
			} else {
				exjson_debug("Cleanup memory: string/double\n");
				/* free string/double */
				::free(m_value);
			}
		}
		/* Set as NULL */
		m_type = ExJSONValNull;
		m_len = 0;
		m_value = NULL;
	}

	/* Int */
	void setInt(const int &i) {alloc(ExJSONValInt); m_len = i; }
	int getInt() const {
		if ((m_type == ExJSONValInt) || (m_type == ExJSONValBool))  return m_len;   /* Native value        */
		else if (m_type == ExJSONValDouble) return ::round((*((double*)m_value)));  /* Round to integer    */
		else if (m_type == ExJSONValString) return atoi(((const char*)m_value));    /* Try to parse string */
		return 0;
	}

	/* Bool */
	void setBool(const bool &i) {alloc(ExJSONValBool); m_len = (int)i; }
	bool getBool() const {
		if ((m_type == ExJSONValBool) || (m_type == ExJSONValInt)) return ((bool)m_len);
		return false;
	}

	/* Double */
	void setDouble(const double &i) {alloc(ExJSONValDouble); *((double *)m_value) = i; }
	double getDouble() const {
		if (m_type == ExJSONValDouble) return (*((double*)m_value));               /* Native value          */
		else if (m_type == ExJSONValInt) return (double)((int)(long long)m_value); /* Convert int to double */
		else if (m_type == ExJSONValString) return atof(((const char*)m_value));   /* Try to parse string   */
		return 0.0;
	}

	/* String */
	void setString(const char *v, int len = -1) {
		if (len <= 0) len = strlen(v);
		clear();
		m_len = len + 1;
		m_type = ExJSONValString;
		m_value = malloc(m_len);
		memcpy(m_value, v, len);
		((char *)m_value)[len] = '\0';
		exjson_debug("Alloc string - copy %d, %s\n", m_value, v);
	}

	/* Array */
	void setArray(const ExJSONValVec &s) {
		clear();
		m_type = ExJSONValArray;
		m_len = sizeof(ExJSONValArray);
		m_value = new ExJSONValVec(s);
		exjson_debug("Alloc Array - copy %d\n", m_value);
	}

	/* Object */
	void setObject(ExJSONValMap &s) {
		clear();
		m_type = ExJSONValObject;
		m_len = sizeof(ExJSONValMap);
		m_value = new ExJSONValMap(s);
	}

public:
	ExJSONValType m_type;
	void         *m_value;
	int           m_len;
};



class ExJSONVal {
public:
	/*!
	 * \brief Constructors.
	 */
	ExJSONVal() { d = std::make_shared<ExJSONData>(); }
	ExJSONVal(ExJSONValType t) { d = std::make_shared<ExJSONData>(); d->alloc(t); }
	ExJSONVal(const ExJSONVal &t) { exjson_debug("Clone pointer (constructor)\n"); d = t.d; }
	ExJSONVal(int v)   {d = std::make_shared<ExJSONData>(); d->setInt(v);    }
	ExJSONVal(bool v)  {d = std::make_shared<ExJSONData>(); d->setBool(v);   }
	ExJSONVal(double v){d = std::make_shared<ExJSONData>(); d->setDouble(v); }
	ExJSONVal(const char *v, int len = -1) {
		d = std::make_shared<ExJSONData>();
		d->setString(v, len);
	}
	ExJSONVal(std::string &s) { ExJSONVal(s.c_str(), s.length()); }
	ExJSONVal(ExJSONValVec &s) {d = std::make_shared<ExJSONData>(); d->setArray(s); }
	ExJSONVal(ExJSONValMap &s) {d = std::make_shared<ExJSONData>(); d->setObject(s); }
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
			ExJSONValMap *v = (ExJSONValMap *)d->m_value;
			while (i != l.end()) {
				const char *key = i->getCString(); i++;
				(*v)[key] = ExJSONVal((*i++));
			}
		} else {
			d->alloc(ExJSONValArray);
			ExJSONValVec *v = (ExJSONValVec *)d->m_value;
			while (i != l.end()) {
				v->push_back(ExJSONVal((*i++)));
				exjson_debug("push val\n");
			}
		}
	}

	/*!
	 * \brief Destructor.
	 */
	~ExJSONVal() { }

	ExJSONVal &operator = ( const ExJSONVal &t ) {
		exjson_debug("Clear and copy\n");
		_detach();
		d->clear();
		d->copy(t.d);
		return *this;
	}

	ExJSONVal &operator = ( ExJSONVal &t ) {
		exjson_debug("Clone pointer (= operator)\n");
		d = t.d;
		return *this;
	}

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
	bool is_null() { return (d->m_type == ExJSONValNull); }

	/* ============--- Integer ---============== */
	ExJSONVal &operator = ( const int &i ) { _detach(); d->setInt(i); return *this; }
	int getInt() const { return d->getInt(); }

	/* ============--- Bool ---============== */
	ExJSONVal &operator = ( const bool &i ) { _detach(); d->setBool(i); return *this; }
	bool getBool() const { return d->getBool(); }


	/* ============--- double ---============== */
	ExJSONVal &operator = ( const double &i ) { _detach(); d->setDouble(i); return *this; }
	double getDouble() const { return d->getDouble(); }

	/* ============--- string ---============== */
	ExJSONVal &operator = ( const char *i ) { _detach(); d->setString(i); return *this; }
	ExJSONVal &operator = ( const std::string &s ) { _detach(); d->setString(s.c_str(), s.length()); return *this; }
	const char *getCString() const {
		if (d->m_type == ExJSONValString) return (const char *)d->m_value; /* Get native value */
		return 0;
	}
	std::string getString() const {
		if (d->m_type == ExJSONValString)
			return std::string((const char *)d->m_value, d->m_len-1);                             /* Get native value.          */
		else if (d->m_type == ExJSONValDouble) return std::to_string(*((double*)d->m_value));     /* Convert double to string.  */
		else if (d->m_type == ExJSONValInt) return std::to_string(d->m_len);                      /* Convert int to string.     */
		else if (d->m_type == ExJSONValBool) return std::string(((bool)d->m_len)?"true":"false"); /* Convert bool to string.    */
		else if ((d->m_type == ExJSONValArray) || (d->m_type == ExJSONValObject)) return dump();  /* Dump to json string.       */
		return std::string();
	}

	/* ============--- Array ---============== */
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
		exjson_debug("Array push_back c string %d, %s\n", d->m_value, v);
	}
	void push_back(std::string &v) {
		ExJSONVal x(v);
		push_back(x);
		exjson_debug("Array push_back string %d %s\n", d->m_value, v.c_str());
	}
	void push_back(ExJSONVal &p) {
		_detach();
		d->alloc(ExJSONValArray);
		ExJSONValVec *v = (ExJSONValVec *)d->m_value;
		v->push_back(std::move(p));
	}
	ExJSONVal& operator[](int i) {
		_detach();
		d->alloc(ExJSONValArray);
		ExJSONValVec *v = (ExJSONValVec *)d->m_value;
		/* Resize vector if needed */
		while (((int)v->size()) < (i + 1)) v->push_back(ExJSONVal());
		exjson_debug("Array set %d, idx = %d\n", d->m_value, i);
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
		_detach();
		d->alloc(ExJSONValObject);
		ExJSONValMap *v = (ExJSONValMap *)d->m_value;
		auto x = v->find(i);
		if (x == v->end()) {
			v->insert({i, ExJSONVal()});
			x = v->find(i);
		}
		return (x->second);
	}
	void setKey(std::string i, ExJSONVal y) {
		_detach();
		d->alloc(ExJSONValObject);
		ExJSONValMap *v = (ExJSONValMap *)d->m_value;
		auto x = v->find(i);
		if (x == v->end()) {
			v->insert({i, y});
		}
	}

	std::string dump() const{ std::string res; res.reserve(128); dumpInt(res); return res; }


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
		return std::move( obj );
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
				n = std::stoi( val ) * std::pow( 10, exp );
			else
				n = std::stoi( val );
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
			case '[' : return std::move( parse_array(str) );
			case '{' : return std::move( parse_object(str) );
			case '\"': return std::move( parse_string(str) );
			case 't' :
			case 'f' : return std::move( parse_bool(str) );
			case 'n' : return std::move( parse_null(str) );
			default  : if( ( value <= '9' && value >= '0' ) || value == '-' )
					return std::move( parse_number(str) );
		}
		return 	ExJSONVal();
	}

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
			case ExJSONValInt:    s.append(std::to_string(d->m_len)); break;
			case ExJSONValBool:   s.append(((bool)d->m_len)?"true":"false"); break;
			case ExJSONValDouble: s.append(std::to_string(*((double *)d->m_value))); break;
			case ExJSONValString: s.append("\"").append(getString()).append("\""); break;
			case ExJSONValArray: {
				s.append("[");
				ExJSONValVec *v = (ExJSONValVec *)d->m_value;
				for (const auto &i: *v) {
					i.dumpInt(s);
					s.append(",");
				}
				if (s.size() > 1) s.resize(s.size() - 1);
				s.append("]");
			} break;
			case ExJSONValObject: {
				s.append("{");
				ExJSONValMap *v = (ExJSONValMap *)d->m_value;
				for (const auto &i: *v) {
					s.append("\"").append(i.first).append("\":");
					i.second.dumpInt(s);
					s.append(",");
				}
				if (s.size() > 1) s.resize(s.size() - 1);
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
