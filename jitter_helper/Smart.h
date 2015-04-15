/*
   Copyright 2010,2011 Michael Fortin / Imported into TML code-base after-the-fact

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#ifndef SMART_H
#define SMART_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*! \file Smart.h	\brief C/C++ Smart Pointers
	
	A list of smart pointers to make memory management much more enjoyable!
*/

/*! \defgroup SmartPointers Smart Pointers for Enjoyable Memory Managment.
	
	Delete is very easy to forget.  To simplify the task, we provide some
	simple smart pointers.  Just don't forget them!
*/

template<class T>
class RCOne;

//! Reference counting
/*! Inherit this object if memory allocation will follow a reference-counting
	scheme */
class RC
{
private:
	//! The number of references.  Initially 0.
	/*! The caller is responsible for initially retaining the object.
		Use RCOne for this task. */
	int m_count;
	
	//! Increases the retain count
	void retain()						{	m_count++;	}
	
	//! Decreases the retain count
	void release()
	{
		m_count--;
		assert(m_count >= 0);
		if (m_count == 0)	delete this;
	}
	
	//! Required...
	template<class T>
	friend class RCOne;

public:
	//! Initializes the retain count to 0
	/*! Must be assigned to a type RCOne */
	RC()	: m_count(0)				{}

	//! A copy is initially initialized with a count of 0
	/*! Must be assigned to a type RCOne */
	RC(const RC &in_o)	: m_count(0)	{}
	
	//! Virtual destructor required
	virtual ~RC()
	{}
};

//! Reference-counted smart pointer
/*! \ingroup SmartPointers
	For more complex memory management schemes, we need a way to specify when
	two objects own a single object.
	
	The template parameter T must be of type RC*/
template<class T>
class RCOne
{
private:
	//! An object that inherits from RC
	T	*m_obj;

public:
	//! Initialize RCOne
	/*!	\param in_obj[in]	Object do retain */
	RCOne(T *in_obj = NULL)
	: m_obj(in_obj)
	{
		if (m_obj)	m_obj->retain();
	}
	
	//! Replace existing reference
	void operator=(T *in_obj)
	{
		T *r = m_obj;
		m_obj = in_obj;
		
		if (m_obj)	m_obj->retain();
		if (r)		r->release();
	}
	
	//! Shortcut to access class members
	/*!	So we can do x->a() instead of x()->a() */
	T *operator->() const
	{
		return m_obj;
	}
	
	//! Access contained object
	T *operator()() const
	{
		return m_obj;
	}
	
	//! Release held reference
	~RCOne()
	{
		if (m_obj)	m_obj->release();
	}
};

//! Holds a single instance of an object.
/*!
\ingroup SmartPointers
A simple smart-pointer.  We have a reference to an object.  When a new
object is added, we delete the previous and save the new reference.

Delete is called on the memory address once the object goes out of scope
or once a different memory address is passed in.

\code
One<Object>		instanceName
instanceName = new Object();	//deletes existing...
if (instanceName())				//true if backed by non-null pointer
\endcode
*/
template<class T>
class One
{
private:
	//! The object.  Set to NULL by default...
	T *m_one;
	
	//! Prevent copies (bad things can happen!)
	One(One &toCopy)	{}

public:
	//! Initialize an instance of the smart pointer
	/*!	\param[in]	in_init	Input pointer.  Optional - default is NULL.
		\pre Input is a valid pointer or NULL
		\post An initialized smart pointer
	*/
	One(T *in_init = NULL)
	: m_one(in_init)
	{}
	
	//! Assign a new memory address to the smart pointer
	/*! \param[in]	in_init	New address.  Can be NULL to delete current data.
		\pre Input is valid pointer or NULL
		\post If current memory address is not NULL, delete is called upon it.
			Then new address is copied over current memory address. 
	*/
	inline T* operator=(T *in_init)
	{
		if (m_one)	delete m_one;
		m_one = in_init;
		
		return m_one;
	}
	
	//! Access internal pointer directly
	/*! For example:
\code
//Create two instances
One<Object> a = new Object();
Object *b = new Object();

//Call the same method on both instances
a()->hello();
b->hello();
\endcode

\pre Stored pointer is not NULL
	*/
	inline T* operator()()	const	{	return m_one;	}
	
	//! Automatic dereferencing of members...
	/*! For example:
\code
//Create two instances
One<Object> a = new Object();
Object *b = new Object();

//Call the same method on both instances
a->hello();
b->hello();
\endcode

\pre Stored pointer is not NULL
*/
	inline T* operator->()	const	{	return m_one;	}
	
	//! Destroys object.
/*! Destroys the object if the internal pointer is not NULL */
	virtual ~One()	{	if(m_one)		delete m_one;	}
};


//! A smart pointer for C++ arrays
/*!
\ingroup SmartPointers
	Since C++ requires arrays of objects to be deleted differently, we
	created this object to worry about the details.
*/
template<class T>
class Many
{
private:
	//! Internal reference
	T *m_many;
	
	//!Prevent copies (bad things can happen!)
	Many(Many &toCopy)	{}

public:
	//! Initializes instance of Many
	/*! \param[in]	in_init		a valid pointer to an array allocated with new.
	*/
	Many(T *in_init = NULL)
	: m_many(in_init)
	{}
	
	//! Assigns a new pointer
	/*!	If there is already a pointer stored in the object, then delete[]
		is called on it.
		\param[in]	in_init		a valid pointer (or NULL) to assign to the object.
	*/
	inline T* operator=(T *in_init)
	{
		if (m_many)	delete []m_many;
		m_many = in_init;
		
		return m_many;
	}
	
	//! Operator to access elements
	inline T& operator[](int in_index)	{	return m_many[in_index];	}
	
	//! Operator used to access the pointer
	inline T* operator()()	const	{	return m_many;	}
	
	//! Operator used to access members
	inline T* operator->()	const	{	return m_many;	}
	
	//! Destroys the instance of the objects stored within.
	virtual ~Many()	{	if(m_many)		delete []m_many;	}
};



#endif
