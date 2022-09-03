#include "api.h"

std::unordered_map<uint32_t, std::string> channelNames =
{
	{662199250, "ArmorPlate"},
	{1367384683, "ArmorSuit"},
	{218592586, "ArmorCloth"},
	{1667433279, "Weapon1"},
	{1667433278, "Weapon2"},
	{1667433277, "Weapon3"},
	{3073305669, "ShipUpper"},
	{3073305668, "ShipDecals"},
	{3073305671, "ShipLower"},
	{1971582085, "SparrowUpper"},
	{1971582084, "SparrowEngine"},
	{1971582087, "SparrowLower"},
	{373026848, "GhostMain"},
	{373026849, "GhostHighlights"},
	{373026850, "GhostDecals"},
};

std::vector<std::string> dataNames =
{
	"\"detail_diffuse_transform\"",
	"\"detail_normal_transform\"",
	"\"spec_aa_transform\"",
	"\"primary_albedo_tint\"",
	"\"primary_emissive_tint_color_and_intensity_bias\"",
	"\"primary_material_params\"",
	"\"primary_material_advanced_params\"",
	"\"primary_roughness_remap\"",
	"\"primary_worn_albedo_tint\"",
	"\"primary_wear_remap\"",
	"\"primary_worn_roughness_remap\"",
	"\"primary_worn_material_parameters\"",
	"\"secondary_albedo_tint\"",
	"\"secondary_emissive_tint_color_and_intensity_bias\"",
	"\"secondary_material_params\"",
	"\"secondary_material_advanced_params\"",
	"\"secondary_roughness_remap\"",
	"\"secondary_worn_albedo_tint\"",
	"\"secondary_wear_remap\"",
	"\"secondary_worn_roughness_remap\"",
	"\"secondary_worn_material_parameters\"",
};

std::vector<std::string> getAPIModelHashes(uint32_t apiHash, std::string packagesPath, std::unordered_map<uint64_t, uint32_t> hash64Table, bool& bSingle)
{
	std::vector<std::string> modelHashes;
	File* modelTable = new File("A67AD080", packagesPath);
	modelTable->getData();
	// Finding val
	uint32_t tableOffset = 0x40;
	uint32_t tableCount;
	memcpy((char*)&tableCount, modelTable->data + 8, 4);
	uint32_t val;

	apiHash = getArtArrangementHash(apiHash, packagesPath);

	for (int i = tableOffset; i < tableOffset + tableCount * 0x20; i += 0x20)
	{
		memcpy((char*)&val, modelTable->data + i, 4);
		if (val == apiHash)
		{
			memcpy((char*)&val, modelTable->data + i + 0x18, 4);
			if (val != 0) modelHashes = getAPIMultiHashes(val + i + 0x18, modelTable, packagesPath, hash64Table);
			else
			{
				uint32_t fHash;
				memcpy((char*)&val, modelTable->data + i + 0x8, 4);
				memcpy((char*)&fHash, modelTable->data + i + 0xC, 4);
				modelHashes = getAPISingleHashes(val, fHash, packagesPath, hash64Table);
				bSingle = true;
			}
			break;
		}
	}
	delete modelTable;
	return modelHashes;
}

uint32_t getArtArrangementHash(uint32_t apiHash, std::string packagesPath)
{
	File* dataTable = new File("AA3FE280", packagesPath);
	dataTable->getData();
	File* arrangementTable = new File("137AD080", packagesPath);
	arrangementTable->getData();

	uint32_t tableOffset = 0x30;
	uint32_t tableCount;
	uint32_t val;
	uint32_t val2;
	memcpy((char*)&tableCount, dataTable->data + 8, 4);
	for (int i = tableOffset; i < tableOffset + tableCount * 0x20; i += 0x20)
	{
		memcpy((char*)&val, dataTable->data + i, 4);
		if (val == apiHash)
		{
			memcpy((char*)&val, dataTable->data + i + 0x10, 4);
			File dataFile = File(uint32ToHexStr(val), packagesPath);
			dataFile.getData();
			memcpy((char*)&val, dataFile.data + 0x88, 4);
			val += 0x88 + 8;
			memcpy((char*)&val2, dataFile.data + val, 4);
			val += val2 + 0x12;
			memcpy((char*)&val2, dataFile.data + val, 2);
			memcpy((char*)&val2, arrangementTable->data + val2 * 4 + 48, 4);
			delete dataTable;
			delete arrangementTable;
			return val2;
		}
	}

	// No art-arrangement hash, let's just try anyway.
	delete arrangementTable;
	return apiHash;
}

void writeShader(std::unordered_map<std::string, std::unordered_map<std::string, std::vector<float>>> dyes, std::unordered_map<std::string, std::unordered_map<std::string, std::string>> textures, bool bCustom, std::string outputPath)
{
	std::string stringFactoryShader = "{\n";
	if (!bCustom) stringFactoryShader += "  \"custom_dyes\": [],\n  \"locked_dyes\": [],\n  \"default_dyes\": [\n";
	else stringFactoryShader += "  \"default_dyes\": [],\n  \"locked_dyes\": [],\n  \"custom_dyes\": [\n";
	std::string propertiesString = "";
	for (auto& it : dyes)
	{
		//propertiesString += "    	" + it.first + ":\n";
		propertiesString += "    {\n";
		propertiesString += "      \"investment_hash\": " + it.first + ",\n";
		if (it.first.find("Cloth", 0) != std::string::npos) propertiesString += "      \"cloth\": true,\n";
		else propertiesString += "      \"cloth\": false,\n";
		propertiesString += "      \"material_properties\": {\n";
		std::string valuesString = "";
		for (auto& it2 : it.second)
		{
			if (it.first.find("diffuse", 0) != std::string::npos) break;
			std::string floatString = "[";
			for (auto& flt : it2.second) floatString += std::to_string(flt) + ", ";
			//stringFactoryShader += "			" + it2.first + ": " + floatString.substr(0, floatString.size()-2) + "],\n";
			valuesString += "        " + it2.first + ": " + floatString.substr(0, floatString.size() - 2) + "],\n";
		}
		propertiesString += valuesString.substr(0, valuesString.size() - 2) + "\n      },\n";
		propertiesString += "      \"textures\": {\n";
		propertiesString += "        \"diffuse\": {\n          \"name\": \"" + textures[it.first]["Diffuse"] + "\"\n        },\n";
		propertiesString += "        \"normal\": {\n          \"name\": \"" + textures[it.first]["Normal"] + "\"\n        }\n";
		propertiesString += "      }\n    },\n";
	}
	stringFactoryShader += propertiesString.substr(0, propertiesString.size() - 2) + "\n  ]\n}";
	//stringFactoryShader += "\n";

	FILE* shaderFile;
	std::string path = outputPath + "/shader.json";
	fopen_s(&shaderFile, path.c_str(), "w");
	fwrite(stringFactoryShader.c_str(), stringFactoryShader.size(), 1, shaderFile);
	fclose(shaderFile);
}


std::vector<std::string> getAPISingleHashes(uint32_t mHash, uint32_t fHash, std::string packagesPath, std::unordered_map<uint64_t, uint32_t> hash64Table)
{
	std::vector<std::string> h64Files = { "", "" };
	File pairTable = File("BEB2C180", packagesPath);
	pairTable.getData();
	uint32_t tableOffset = 0x30;
	uint32_t tableCount;
	memcpy((char*)&tableCount, pairTable.data + 8, 4);
	uint32_t val;
	for (int i = tableOffset; i < tableOffset + tableCount * 8; i += 8)
	{
		memcpy((char*)&val, pairTable.data + i, 4);
		if (val == mHash)
		{
			memcpy((char*)&val, pairTable.data + i + 4, 4);
			h64Files[0] = uint32ToHexStr(val);
		}
		else if (val == fHash)
		{
			memcpy((char*)&val, pairTable.data + i + 4, 4);
			h64Files[1] = uint32ToHexStr(val);
		}
		if (h64Files[0] != "" && h64Files[1] != "") break;
	}

	return getHashesFromH64s(h64Files, packagesPath, hash64Table);;
}

std::vector<std::string> getAPIMultiHashes(uint32_t tableOffset, File* modelTable, std::string packagesPath, std::unordered_map<uint64_t, uint32_t> hash64Table)
{
	uint32_t val1;
	uint32_t val2;
	std::vector<uint32_t> offsets;
	std::vector<std::string> h64Files;
	std::vector<std::uint32_t> pairFiles;
	uint32_t tableCount;
	memcpy((char*)&tableCount, modelTable->data + tableOffset, 4);
	tableOffset += 0x10;
	for (int i = tableOffset; i < tableOffset + tableCount * 8; i += 8)
	{
		memcpy((char*)&val1, modelTable->data + i, 4);
		val1 += i + 0x10;
		memcpy((char*)&val2, modelTable->data + val1, 4);
		val2 += val1;
		uint32_t tableCount2;
		memcpy((char*)&tableCount2, modelTable->data + val2, 4);
		val2 += 0x10;
		uint32_t val3;
		for (int j = val2; j < val2 + tableCount2 * 4; j += 4)
		{
			memcpy((char*)&val3, modelTable->data + j, 4);
			pairFiles.push_back(val3);
		}
	}

	File pairTable = File("BEB2C180", packagesPath);
	pairTable.getData();
	uint32_t table2Offset = 0x30;
	uint32_t table2Count;
	memcpy((char*)&table2Count, pairTable.data + 8, 4);
	uint32_t val;
	for (int i = table2Offset; i < table2Offset + table2Count * 8; i += 8)
	{
		memcpy((char*)&val, pairTable.data + i, 4);
		for (auto& hash : pairFiles)
		{
			if (val == hash)
			{
				memcpy((char*)&val, pairTable.data + i + 4, 4);
				h64Files.push_back(uint32ToHexStr(val));
			}
		}
	}

	return getHashesFromH64s(h64Files, packagesPath, hash64Table);
}