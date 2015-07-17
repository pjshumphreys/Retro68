#include "ResourceFile.h"
#include "BinaryIO.h"

#include <boost/filesystem.hpp>
#include "boost/filesystem/fstream.hpp"
#include <sstream>
#include <iostream>

#ifdef __APPLE__
#include <sys/xattr.h>
#endif
extern "C" {
#include "hfs.h"
}

namespace fs = boost::filesystem;

// CRC 16 table lookup array
static unsigned short CRC16Table[256] =
	{0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

// CalculateCRC
static unsigned short CalculateCRC(unsigned short CRC, const char* dataBlock, int dataSize)
{
	while (dataSize)
	{
		CRC = (CRC << 8) ^ CRC16Table[((*dataBlock) ^ (CRC >> 8)) & 0x00FF];
		dataBlock++;
		dataSize--;
	}

	return CRC;
}

static void writeMacBinary(std::ostream& out, std::string filename,
						   ResType type, ResType creator,
						   const Resources& rsrc, const std::string& data)
{
	out.seekp(128);
	out << data;
	std::streampos dataend = out.tellp();
	std::streampos rsrcstart = ((int)dataend + 0x7F) & ~0x7F;
	rsrc.writeFork(out);

	std::streampos rsrcend = out.tellp();
	while((int)out.tellp() % 128)
		byte(out,0);

	std::ostringstream header;
	byte(header, 0);
	byte(header, filename.size());
	header << filename;
	while((int)header.tellp() < 65)
		byte(header,0);
	ostype(header, type);
	ostype(header, creator);
	byte(header, 0); // flags
	byte(header, 0);
	word(header, 0);  // position.v
	word(header, 0); // position.h
	word(header, 0); // folder id
	byte(header, 0); // protected flag
	byte(header, 0);
	longword(header, ((int)dataend - 128));
	longword(header, (int) (rsrcend - rsrcstart));
	longword(header, 0); // creation date
	longword(header, 0); // modification date
	while((int)header.tellp() < 124)
		byte(header,0);

	out.seekp(0);
	std::string headerData = header.str();
//   out.write(&headerData[0], headerData.size());
	out << headerData;
	word(out, CalculateCRC(0, &headerData[0], headerData.size()));
	word(out, 0);
	//longword(out,0);
	out.seekp(0, std::ios::end);
}



ResourceFile::ResourceFile()
{
}

ResourceFile::ResourceFile(std::string path, ResourceFile::Format f)
{
	assign(path, f);
}

ResourceFile::~ResourceFile()
{

}

bool ResourceFile::assign(std::string pathstring, ResourceFile::Format f)
{
	this->pathstring = pathstring;
	fs::path path(pathstring);

	fs::path rsrcPath = path.parent_path() / ".rsrc" / path.filename();
	fs::path finfPath = path.parent_path() / ".finf" / path.filename();

	format = f;
	if(format == Format::autodetect)
	{
		if(path.extension() == ".bin")
			format = Format::macbin;
		else if(path.extension() == ".dsk" || path.extension() == ".img")
			format = Format::diskimage;
		//else if(fs::exists(rsrcPath))
		//	format = Format::basilisk;
		else
		{
			fs::ifstream in(path);
			if(in)
			{
				int magic1 = longword(in);
				if(in && magic1 == 0x00051600)
				{
					int magic2 = longword(in);
					if(in && magic2 == 0x00020000)
						format = Format::applesingle;
				}
			}
		}
	}
	if(format == Format::autodetect)
	{
#ifdef __APPLE__
		format = Format::real;
#else
		format = Format::basilisk;
#endif
	}
//	std::cout << "assigned: " << pathstring << " format " << (int)format << "\n";
	return true;
}

bool ResourceFile::read()
{
	fs::path path(pathstring);

	switch(format)
	{
		case Format::basilisk:
			{
				fs::ifstream dataIn(path);
				data = std::string(std::istreambuf_iterator<char>(dataIn),
								   std::istreambuf_iterator<char>());

				fs::ifstream rsrcIn(path.parent_path() / ".rsrc" / path.filename());
				resources = Resources(rsrcIn);
				fs::ifstream finfIn(path.parent_path() / ".finf" / path.filename());
				type = ostype(finfIn);
				creator = ostype(finfIn);
			}
			break;
#ifdef __APPLE__
		case Format::real:
			{
				fs::ifstream dataIn(path);
				data = std::string(std::istreambuf_iterator<char>(dataIn),
								   std::istreambuf_iterator<char>());
				fs::ifstream rsrcIn(path / "..namedfork" / "rsrc");
				resources = Resources(rsrcIn);

				char finf[32];
				int n = getxattr(path.c_str(), XATTR_FINDERINFO_NAME,
						finf, 32, 0, 0);

				std::istringstream finfIn(std::string(finf, finf+n));
				type = ostype(finfIn);
				creator = ostype(finfIn);
			}
			break;
#endif
		case Format::applesingle:
			{
				fs::ifstream in(path);
				if(longword(in) != 0x00051600)
					return false;
				if(longword(in) != 0x00020000)
					return false;
				in.seekg(24);
				int n = word(in);
				for(int i = 0; i < n; i++)
				{
					in.seekg(26 + i * 12);
					int what = longword(in);
					int off = longword(in);
					//int len = longword(in);
					in.seekg(off);
					switch(what)
					{
						case 1:
							// ###
							break;
						case 2:
							resources = Resources(in);
							break;
						case 9:
							type = ostype(in);
							creator = ostype(in);
							break;
					}
				}
			}
			break;
		case Format::macbin:
			{
				fs::ifstream in(path);
				if(byte(in) != 0)
					return false;
				if(byte(in) > 63)
					return false;
				in.seekg(65);
				type = ostype(in);
				creator = ostype(in);
				in.seekg(83);
				int datasize = longword(in);
				//int rsrcsize = longword(in);

				in.seekg(0);
				char header[124];
				in.read(header, 124);
				unsigned short crc = CalculateCRC(0,header,124);
				if(word(in) != crc)
					return false;
				in.seekg(128 + datasize);
				resources = Resources(in);
			}
			break;
		default:
			return false;
	}
	return true;
}

bool ResourceFile::write()
{
	fs::path path(pathstring);

	switch(format)
	{
		case Format::basilisk:
			{
				fs::create_directory(path.parent_path() / ".rsrc");
				fs::create_directory(path.parent_path() / ".finf");

				fs::ofstream dataOut(path);
				fs::ofstream rsrcOut(path.parent_path() / ".rsrc" / path.filename());
				fs::ofstream finfOut(path.parent_path() / ".finf" / path.filename());

				dataOut << data;
				resources.writeFork(rsrcOut);

				ostype(finfOut, type);
				ostype(finfOut, creator);
				for(int i = 8; i < 32; i++)
					byte(finfOut, 0);
			}
			break;
#ifdef __APPLE__
		case Format::real:
			{
				fs::ofstream dataOut(path);
				fs::ofstream rsrcOut(path / "..namedfork" / "rsrc");
				std::ostringstream finfOut;

				dataOut << data;
				resources.writeFork(rsrcOut);

				ostype(finfOut, type);
				ostype(finfOut, creator);
				for(int i = 8; i < 32; i++)
					byte(finfOut, 0);
				setxattr(path.c_str(), XATTR_FINDERINFO_NAME,
						finfOut.str().data(), 32, 0, 0);
			}
			break;
#endif
		case Format::macbin:
			{
				fs::ofstream out(path);
				writeMacBinary(out, path.stem().string(), type, creator, resources, data);
			}
			break;
		case Format::applesingle:
			{
				fs::ofstream out(path);
				longword(out, 0x00051600);
				longword(out, 0x00020000);
				for(int i = 0; i < 16; i++)
					byte(out, 0);
				word(out, 3);
				std::streampos entries = out.tellp();
				for(int i = 0; i < 3*3; i++)
					longword(out, 0);
				std::streampos dataStart = out.tellp();
				out << data;
				std::streampos rsrcStart = out.tellp();
				resources.writeFork(out);
				std::streampos finfStart = out.tellp();
				ostype(out, type);
				ostype(out, creator);
				for(int i = 8; i < 32; i++)
					byte(out, 0);
				out.seekp(entries);
				longword(out, 1);
				longword(out, dataStart);
				longword(out, rsrcStart - dataStart);
				longword(out, 2);
				longword(out, rsrcStart);
				longword(out, finfStart - rsrcStart);
				longword(out, 3);
				longword(out, finfStart);
				longword(out, 32);
			}
			break;
		case Format::diskimage:
			{
				std::ostringstream rsrcOut;
				resources.writeFork(rsrcOut);
				std::string rsrc = rsrcOut.str();
				int size = rsrc.size();

				size += 20 * 1024;
				size += 800*1024 - size % (800*1024);

				fs::ofstream(path, std::ios::binary | std::ios::trunc).seekp(size-1).put(0);

				hfs_format(pathstring.c_str(), 0, 0, path.stem().string().substr(0,27).c_str(), 0, NULL);
				hfsvol *vol = hfs_mount(pathstring.c_str(), 0, HFS_MODE_RDWR);
				//hfs_setvol(vol, )
				hfsfile *file = hfs_create(vol, (path.stem().string().substr(0,31)).c_str(),
										   ((std::string)type).c_str(), ((std::string)creator).c_str());
				hfs_setfork(file, 0);
				hfs_write(file, data.data(), data.size());
				hfs_setfork(file, 1);
				hfs_write(file, rsrc.data(), rsrc.size());

				hfs_close(file);
				hfs_umount(vol);

			}
			break;

		default:
			return false;
	}
	return true;
}
