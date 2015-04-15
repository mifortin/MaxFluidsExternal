/*
   Copyright 2011 Michael Fortin	/	Added after the fact to TML code-base

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

#ifndef SMARTMM_H
#define SMARTMM_H

/*! \file SmartMM.h
	\brief Smart pointers to help deal with Objective-C memory management
	
	Retaining and releasing Objective-C objects is menial labour
	that can be automated using C++.  We use a C++ constructor (or assignment)
	to retain and the destructor to release.  Optimally, this object is inlined
	providing no degradation in performance compared to manually doing the
	retain and releasing in code.
	
	Realistically, the overhead is calling autorelease on various objects and
	that the Objective-C bridge, as I understand it, creates a method for
	initialization and destruction of C++ objects.  As such, in Objective-C,
	there is a slight overhead in run-time.
	
	The slight overhead is nothing compared to the time saved by reducing
	the chances of making mistakes with retaining and releasing.
*/

//! Smart pointer for Objective-C objects
/*!
	\ingroup SmartPointers
	
	This object takes an Objective-C object and automatically calls retain
	an release on it.  Passed in objects are assumed to be autoreleased.
	
	\code
//Create a string set to be released by the current auto-release pool
NSString *myString = [NSString stringWithFormat:@"%s %s", "Hello", "World"];

{
	//Implicit retain of myString (through constructor)
	OneNS<NSString> myRef(myString);
	
	//Call methods on string...
	[myRef() length];
	
	//Implicit release of myString (through destructor)
}
	\endcode
*/
template<class T>
class OneNS
{
private:
	//! Private reference to Objective-C object
	T	m_one;

public:
	//! Copy constructor
	/*! \param cpy[in]		OneNS object to copy */
	OneNS(const OneNS &cpy)
	{
		m_one = cpy.m_one;
		if (m_one)		[m_one retain];
	}
	
	//! Create a new smart pointer
	/*! \param in_one[in]	Default value of nil, a reference of the object to retain */
	OneNS(T in_one = nil)
	{
		m_one = in_one;
		if (m_one)		[m_one retain];
	}
	
	//! Assignment operator...
	/*!	\param	in_one[in]		Value to assign.
		\return	reference to instance*/
	OneNS<T> &operator=(const OneNS &in_one)
	{
		this->operator=(in_one());
		return *this;
	}
	
	//! Assign a different pointer
	/*! \param in_init[in]	New reference to retain or nil
		\return	The previous reference and releasing it once. */
	inline T operator=(T in_init)
	{
		if (in_init)	[in_init retain];
		if (m_one)		[m_one release];
		
		m_one = in_init;
		
		return m_one;
	}
	
	//! Compare two references.
	/*! \param in_cmp[in]	Reference to compare
		\return true if the addresses of the internal reference and in_cmp
				are the same. */
	inline bool operator==(T in_cmp)
	{
		return in_cmp == m_one;
	}
	
	//! Not equal
	/*! \param in_cmp[in]	Reference to compare with
		\return	true if the address is different */
	inline bool operator!=(T in_cmp)
	{
		return in_cmp != m_one;
	}
	
	//! Overloaded function operator
	/*! Use this to access the value of the underlying objective-C reference. */
	inline T operator()() const		{	return m_one;	}
	
	//! Destructor releases held instance.
	virtual ~OneNS()
	{
		if (m_one)		[m_one release];
	}
};

#endif