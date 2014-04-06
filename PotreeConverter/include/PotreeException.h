
#ifndef POTREEEXCEPTION_H
#define POTREEEXCEPTION_H

// using standard exceptions
#include <iostream>
#include <exception>
#include <string>

using std::exception;
using std::string;

class PotreeException: public exception{
private:
	string message;

public:
	PotreeException(string message){
		this->message = message;
	}

	virtual const char* what() const throw(){    
		return message.c_str();
	}
};


#endif