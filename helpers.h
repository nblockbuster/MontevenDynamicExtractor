#pragma once
#include <string>
#include "../DestinyUnpackerCPP/package.h"

enum PrimitiveType
{
	Triangles = 3,
	TriangleStrip = 5
};

enum BufferType
{
	VertPrimary,
	VertSecondary,
	OldWeights,
	Unk,
	Index,
	VertColour,
	SPSBWeights
};

class File
{
private:

public:
	File(std::string x);

	std::string id = "";
	unsigned char* data = nullptr;
	std::string pkgName = "";

	unsigned char* getData();
	std::string getPkgID();
};

class Header : public File
{
private:
	Header();

public:
};

class TextureHeader : public Header
{
private:

public:
};

class Material : public File
{
private:

public:
};

class Model : public File
{
private:

public:
};

class Mesh
{
private:

public:
};

class DynamicMesh : public Mesh
{
private:

public:
};

class Submesh
{
private:

public:
};

class DynamicSubmesh : public Submesh
{
private:

public:
};

std::string getReferenceFromHash(std::string packagesPath, std::string pkgID, std::string hash);