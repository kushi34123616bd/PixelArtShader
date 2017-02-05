#pragma once

class BMP;

class CPngRW
{
public:
	static bool LoadPng( const char *szFilename, BMP &bmp );
	static bool SavePng( const char *szFilename, BMP &bmp );
};
