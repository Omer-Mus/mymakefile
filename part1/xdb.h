/*
 * xdb.h
 */

#pragma once

#include <fstream>
#include <vector>
#include <system_error>

namespace xdb {

template <class T>
class XdbReader {

public:
	XdbReader(const char *path);

	size_t size() const { return list.size(); }
	const T& operator[](int i) const { return list[i]; };

protected:
	std::vector<T> list;

};

template <class T>
class XdbWriter {

public:
	XdbWriter(const char *path, bool append = false);

	XdbWriter& operator<<(const T& rec);

private:
	std::ofstream ofs;

};

template <class T>
class Xdb : public XdbWriter<T>, public XdbReader<T>
{

public:
	Xdb(const char *path);

	Xdb& operator<<(const T& rec);

};

template <class T>
XdbReader<T>::XdbReader(const char *path)
{
	std::ifstream ifs(path, std::ifstream::in |
	                        std::ifstream::binary);
	if (ifs.fail())
		throw std::system_error(errno, std::generic_category(),
		                        "std::ifstream");

	T rec;
	while (ifs >> rec)
		list.push_back(rec);
	if (ifs.bad())
		throw std::system_error(errno, std::generic_category(),
		                        "std::ifstream::operator>>");
}

template <class T>
XdbWriter<T>::XdbWriter(const char *path, bool append)
	: ofs(path, (append ? std::ofstream::app : std::ofstream::out)
	            | std::ofstream::binary)
{
	if (ofs.fail())
		throw std::system_error(errno, std::generic_category(),
		                        "std::ofstream");
}

template <class T>
Xdb<T>::Xdb(const char *path)
	: XdbWriter<T>(path, true), XdbReader<T>(path)
{}

template <class T>
XdbWriter<T>& XdbWriter<T>::operator<<(const T& rec)
{
	if ((ofs << rec).bad())
		throw std::system_error(errno, std::generic_category(),
		                        "std::ofstream::operator<<");
	return *this;
}

template <class T>
Xdb<T>& Xdb<T>::operator<<(const T& rec)
{
	static_cast<XdbWriter<T>&>(*this) << rec;
	this->list.push_back(rec);
	return *this;
}

} // namespace xdb
