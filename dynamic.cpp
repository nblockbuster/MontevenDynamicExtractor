#include "dynamic.h"

bool Dynamic::get()
{
	fbxModel = new FbxModel();
	if (getReferenceFromHash(hash, packagesPath) != "d89a8080")
	{
		std::cerr << "Given hash is not a valid dynamic model.";
		return false;
	}
	getData();
	getDyn3Files();
	if (!dyn3s.size())
	{
		std::cerr << "Dynamic has no model data attached.";
		return false;
	}
	parseDyn3s();
	considerSkeletonOverride();
	if (skeletonHash != "")
		getSkeleton();
	if (bTextures)
		getTexturePlates();
	getSubmeshes();
	return meshes.size();
}

void Dynamic::considerSkeletonOverride()
{
	if (skeletonOverride != -1)
	{
		switch (skeletonOverride)
		{
		case 0:
			printf("Skeleton flag 0, no skeleton will be ripped.");
			skeletonHash = "";
			break;
		case 1:
			printf("Skeleton flag 1, ripped models will use the player body rig.");
			skeletonHash = "BC38AB80"; //this is eva's body rig but could be the same
			break;
		case 2:
			printf("Skeleton flag 2, ripped models will use the player face rig.");
			skeletonHash = "9018AB80"; //this is ikora's head rig but... might be the same?
			break;
		}
	}
}

void Dynamic::getTexturePlates()
{
	uint32_t offset;
	uint32_t fileVal;
	std::string fileHash;
	for (auto& dyn2 : dyn2s)
	{
		// can optimise by replacing with pointers
		//dyn2->getData();
		int fileSize = dyn2->getData();
		if (!fileSize) continue;
		memcpy((char*)&offset, dyn2->data + 0x18, 4);
		offset += 808;
		if (offset >= fileSize)
			continue;
		memcpy((char*)&fileVal, dyn2->data + offset, 4);
		if (fileVal < 0x80a00000 || fileVal > 0x81ffffff) continue;
		fileHash = uint32ToHexStr(fileVal);
		TexturePlateSet* texplateSet = new TexturePlateSet(fileHash, packagesPath);
		texplateSets.push_back(texplateSet);
	}
}

void Dynamic::getDyn3Files()
{
	uint32_t off;
	int fileSize;

	uint32_t primFileID;
	memcpy((char*)&primFileID, data + 0xB0, 4);
	File* primFile = new File(uint32ToHexStr(primFileID), packagesPath);
	fileSize = primFile->getData();
	if (!fileSize) return;
	// Finding 42868080
	bool bSkeleton = false;
	memcpy((char*)&off, primFile->data + 0x18, 4);
	off += 0x18;
	memcpy((char*)&off, primFile->data + off + 4, 4);
	// 2155905493 has only inverse data
	if (off == 0x808081DD || off == 0x808081D5)
	{
		if (off == 0x808081D5) bSkeletonDiostOnly = true;
		bSkeleton = true;
	}
	if (bSkeleton)
	{
		skeletonHash = primFile->hash;
		memcpy((char*)&off, data + 0xBC, 4);
		dyn2s.push_back(new File(uint32ToHexStr(off), packagesPath));
		memcpy((char*)&off, data + 0xC8, 4);
		dyn2s.push_back(new File(uint32ToHexStr(off), packagesPath));
	}
	else
	{
		memcpy((char*)&off, data + 0xB0, 4);
		dyn2s.push_back(new File(uint32ToHexStr(off), packagesPath));
		memcpy((char*)&off, data + 0xBC, 4);
		dyn2s.push_back(new File(uint32ToHexStr(off), packagesPath));
	}
	memcpy((char*)&off, data + 0xA0, 4);
	if (off == 1) dyn2s.pop_back(); // If the array size is 1 just delete the second dyn2

	std::vector<std::string> existingDyn3s = {};
	for (int i = 0; i < dyn2s.size(); i++)
	{
		File* dyn2 = primFile;
		// We don't need to reopen the primFile if there isnt a skeleton
		if (bSkeleton)
		{
			dyn2 = dyn2s[i];
			fileSize = dyn2->getData();
			if (fileSize == 0)
			{
				printf("\nDynamic has no mesh data (B), skipping...");
				dyn2s.erase(dyn2s.begin() + i);
				i--;
				continue;
			}
		}
		else
		{
			dyn2 = dyn2s[i];
			fileSize = dyn2->getData();
			if (fileSize == 0)
			{
				printf("\nDynamic has no mesh data (B), skipping...");
				dyn2s.erase(dyn2s.begin() + i);
				i--;
				continue;
			}
		}

		memcpy((char*)&off, dyn2->data + 0x18, 4);
		if (off + 572 - 4 >= fileSize)
		{
			printf("\nDynamic has no mesh data (C), skipping...");
			continue;
		}
		memcpy((char*)&off, dyn2->data + off + 572, 4);
		if (off < 0x80800000)
		{
			printf("\nDynamic has no mesh data (D), skipping...");
			continue;
		}
		File* dyn3 = new File(uint32ToHexStr(off), packagesPath);
		if (std::find(existingDyn3s.begin(), existingDyn3s.end(), dyn3->hash) != existingDyn3s.end()
			|| dyn3->getData() == 0)
		{
			dyn2s.erase(dyn2s.begin() + i);
			i--;
			continue;
		}
		else
		{
			existingDyn3s.push_back(dyn3->hash);
			dyn3s.push_back(dyn3);
		}

		// External material table
		if (bTextures)
		{
			uint32_t extOff = 0;
			memcpy((char*)&extOff, dyn2->data + 0x48, 4);
			extOff += 0x48 - 8;
			uint32_t val;
			bool bFound = false;
			while (true)
			{
				memcpy((char*)&val, dyn2->data + extOff, 4);
				if (val == 0x80800014)
				{
					bFound = true;
					extOff -= 8;
					break;
				}
				else if (val == 0x80809FB8) break;
				extOff -= 4;
			}
			if (bFound)
			{
				uint32_t extCount;
				memcpy((char*)&extCount, dyn2->data + extOff, 4);
				extOff += 0x10;
				std::set<std::uint32_t> existingMats;
				for (int j = extOff; j < extOff + extCount * 4; j += 4)
				{
					memcpy((char*)&val, dyn2->data + j, 4);
					if (existingMats.find(val) == existingMats.end())
					{
						Material* mat = new Material(uint32ToHexStr(val), packagesPath);
						externalMaterials.push_back(mat);
						existingMats.insert(val);
					}
				}
			}
		}
	}
}

void Dynamic::parseDyn3s()
{
	uint32_t offset;
	uint32_t count;
	int fileSize;

	for (int i = 0; i < dyn3s.size(); i++)
	{
		File* dyn3 = dyn3s[i];
		// We've already gotten data for them
		memcpy((char*)&offset, dyn3->data + 0x18, 4);
		offset += 0x18 + 0x10;
		memcpy((char*)&count, dyn3->data + 0x10, 4);
		for (int j = offset; j < offset + count * 0x80; j += 0x80)
		{
			uint32_t off;
			DynamicMesh* mesh = new DynamicMesh();
			memcpy((char*)&off, dyn3->data + j + 0x10, 4);
			mesh->facesFile = new IndexBufferHeader(uint32ToHexStr(off), packagesPath);
			memcpy((char*)&off, dyn3->data + j, 4);
			mesh->vertPosFile = new VertexBufferHeader(uint32ToHexStr(off), packagesPath, VertPrimary);
			// UV
			memcpy((char*)&off, dyn3->data + j + 0x4, 4);
			std::string uvHash = uint32ToHexStr(off);
			if (uvHash != "ffffffff")
			{
				mesh->vertUVFile = new VertexBufferHeader(uvHash, packagesPath, VertSecondary);
			}
			memcpy((char*)&off, dyn3->data + j + 0x8, 4);
			std::string oldWeightsHash = uint32ToHexStr(off);
			if (oldWeightsHash != "ffffffff")
			{
				mesh->oldWeightsFile = new VertexBufferHeader(oldWeightsHash, packagesPath, OldWeights);
			}
			memcpy((char*)&off, dyn3->data + j + 0x14, 4);
			std::string vcHash = uint32ToHexStr(off);
			if (vcHash != "ffffffff")
			{
				mesh->vertColFile = new VertexBufferHeader(vcHash, packagesPath, VertColour);
			}
			memcpy((char*)&off, dyn3->data + j + 0x18, 4);
			std::string spsbHash = uint32ToHexStr(off);
			if (spsbHash != "ffffffff")
			{
				mesh->spsbWeightsFile = new VertexBufferHeader(spsbHash, packagesPath, SPSBWeights);
			}

			uint32_t submeshTableCount;
			memcpy((char*)&submeshTableCount, dyn3->data + j + 0x20, 4);
			uint32_t submeshTableOffset;
			memcpy((char*)&submeshTableOffset, dyn3->data + j + 0x28, 4);
			submeshTableOffset += j + 0x28 + 0x10;

			int currentLOD = 999;
			int lodGroup = 0;
			for (int k = submeshTableOffset; k < submeshTableOffset + submeshTableCount * 0x24; k += 0x24)
			{
				DynamicSubmesh* submesh = new DynamicSubmesh();
				int val;
				memcpy((char*)&val, dyn3->data + k, 4);
				std::string materialHash = uint32ToHexStr(val);
				if (materialHash != "ffffffff")
					submesh->material = new Material(materialHash, packagesPath);


				memcpy((char*)&submesh->primType, dyn3->data + k + 6, 2);
				memcpy((char*)&submesh->indexOffset, dyn3->data + k + 0x8, 4);
				memcpy((char*)&submesh->indexCount, dyn3->data + k + 0xC, 4);
				memcpy((char*)&submesh->lodLevel, dyn3->data + k + 0x1D, 1);
				if (submesh->lodLevel < currentLOD) lodGroup++;
				currentLOD = submesh->lodLevel;
				submesh->lodGroup = lodGroup;
				mesh->submeshes.push_back(submesh);
			}

			PrimitiveType primType = mesh->submeshes[0]->primType;

			mesh->vertPosFile->vertexBuffer->getVerts(mesh);
			transformPos(mesh, dyn3->data);

			if (mesh->vertUVFile)
			{
				mesh->vertUVFile->vertexBuffer->getVerts(mesh);
				transformUV(mesh, dyn3->data);
			}

			if (mesh->oldWeightsFile)
			{
				mesh->oldWeightsFile->vertexBuffer->getVerts(mesh);
			}

			if (mesh->vertColFile)
			{
				mesh->vertColFile->vertexBuffer->getVerts(mesh);
			}

			if (mesh->spsbWeightsFile)
			{
				mesh->spsbWeightsFile->vertexBuffer->getVerts(mesh);
			}

			mesh->facesFile->indexBuffer->getFaces(mesh, primType);

			meshes.push_back(mesh);
		}
	}
}


void Dynamic::transformPos(DynamicMesh* mesh, unsigned char* data)
{
	float scale;
	memcpy((char*)&scale, data + 108, 4);
	std::vector<float> offset;
	for (int i = 96; i < 108; i += 4)
	{
		float val;
		memcpy((char*)&val, data + i, 4);
		offset.push_back(val);
	}
	for (auto& vert : mesh->vertPos)
	{
		for (int i = 0; i < 3; i++)
		{
			vert[i] = vert[i] * scale + offset[i];
		}
	}
}

void Dynamic::transformUV(DynamicMesh* mesh, unsigned char* data)
{
	std::vector<float> scales;
	std::vector<float> offset;
	for (int i = 112; i < 120; i += 4)
	{
		float val;
		memcpy((char*)&val, data + i, 4);
		scales.push_back(val);
	}
	for (int i = 120; i < 128; i += 4)
	{
		float val;
		memcpy((char*)&val, data + i, 4);
		offset.push_back(val);
	}
	for (auto& vert : mesh->vertUV)
	{
		vert[0] = vert[0] * scales[0] + offset[0];
		vert[1] = vert[1] * -scales[1] + (1 - offset[1]);
	}
}

void Dynamic::getSubmeshes()
{
	for (DynamicMesh* mesh : meshes)
	{
		//std::vector<uint32_t> existingOffsets;
		std::unordered_map<uint32_t, int> existingSubmeshes;
		for (DynamicSubmesh* submesh : mesh->submeshes)
		{
			// Removing dupes
			//if (std::find(existingOffsets.begin(), existingOffsets.end(), submesh->indexOffset) != existingOffsets.end())
			if (existingSubmeshes.find(submesh->indexOffset) != existingSubmeshes.end())
			{
				if (submesh->lodLevel >= existingSubmeshes[submesh->indexOffset]) continue;
			}

			// Potential memory leaking
			if (submesh->primType == TriangleStrip)
			{
				submesh->faces.reserve(mesh->faceMap[submesh->indexOffset + submesh->indexCount + 1] - mesh->faceMap[submesh->indexOffset]);
				for (std::size_t i = mesh->faceMap[submesh->indexOffset]; i < mesh->faceMap[submesh->indexOffset + submesh->indexCount + 1]; ++i) {
					submesh->faces.emplace_back(mesh->faces[i].begin(), mesh->faces[i].end());
				}
			}
			else
			{
				submesh->faces.reserve(floor((submesh->indexCount) / 3));
				for (std::size_t i = floor(submesh->indexOffset / 3); i < floor((submesh->indexOffset + submesh->indexCount) / 3); ++i) {
					submesh->faces.emplace_back(mesh->faces[i].begin(), mesh->faces[i].end());
				}
			}

			// Code to move faces down to zero
			std::set<int> dsort;
			for (auto& face : submesh->faces)
			{
				for (auto& f : face)
				{
					dsort.insert(f);
				}
			}
			if (!dsort.size()) continue;

			std::unordered_map<int, int> d;
			int i = 0;
			for (auto& val : dsort)
			{
				d[val] = i;
				i++;
			}
			for (auto& face : submesh->faces)
			{
				for (auto& f : face)
				{
					f = d[f];
				}
			}

			// Trimming verts to minimise file size
			submesh->vertPos = trimVertsData(mesh->vertPos, dsort, false);
			if (mesh->vertNorm.size()) submesh->vertNorm = trimVertsData(mesh->vertNorm, dsort, false);
			if (mesh->vertUV.size()) submesh->vertUV = trimVertsData(mesh->vertUV, dsort, false);
			if (mesh->vertCol.size()) submesh->vertCol = trimVertsData(mesh->vertCol, dsort, true);
			if (mesh->weights.size()) submesh->weights = trimVertsData(mesh->weights, dsort, false); //hash 5BF4DE80 is failing on trimming the weights data.
			if (mesh->weightIndices.size()) submesh->weightIndices = trimVertsData(mesh->weightIndices, dsort);
			//existingOffsets.push_back(submesh->indexOffset);
			existingSubmeshes[submesh->indexOffset] = submesh->lodLevel;

			// Vertex colour slots
			addVertColSlots(mesh, submesh);
			submesh->vertColSlots = trimVertsData(submesh->vertColSlots, dsort, true);
		}
	}
}

void Dynamic::addVertColSlots(DynamicMesh* mesh, DynamicSubmesh* submesh)
{
	if (mesh->bCloth)
	{
		std::vector<float> vc = { 0., 0., 0., 1. };
		switch (submesh->gearDyeChangeColourIndex)
		{
		case 0:
			vc[0] = 0.333;
			break;
		case 1:
			vc[0] = 0.666;
			break;
		case 2:
			vc[0] = 0.999;
			break;
		case 3:
			vc[1] = 0.333;
			break;
		case 4:
			vc[1] = 0.666;
			break;
		case 5:
			vc[1] = 0.999;
			break;
		}
		if (submesh->alphaClip) vc[2] = 0.25;
		for (int i = 0; i < submesh->vertPos.size(); i++)
			submesh->vertColSlots.push_back(vc);
	}
	else
	{
		// TODO We can partially pull this out since its per-mesh, not per-submesh
		for (auto& w : mesh->vertNormW)
		{
			std::vector<float> vc = { 0., 0., 0., 1. };
			switch (w & 0x7)
			{
			case 0:
				vc[0] = 0.333;
				break;
			case 1:
				vc[0] = 0.666;
				break;
			case 2:
				vc[0] = 0.999;
				break;
			case 3:
				vc[1] = 0.333;
				break;
			case 4:
				vc[1] = 0.666;
				break;
			case 5:
				vc[1] = 0.999;
				break;
			}
			if (submesh->alphaClip) vc[2] = 0.25;
			submesh->vertColSlots.push_back(vc);
		}
	}
}

void Dynamic::getSkeleton()
{
	Skeleton* skeleton = new Skeleton(skeletonHash, packagesPath, bSkeletonDiostOnly);
	std::vector<Node*> nodes = skeleton->get();
	if (!nodes.size()) return;

	for (auto& node : nodes)
	{
		FbxSkeleton* fbxSkel = FbxSkeleton::Create(fbxModel->manager, node->name.c_str());
		fbxSkel->SetSkeletonType(FbxSkeleton::eLimbNode);
		fbxSkel->Size.Set(node->dost->scale);
		node->fbxNode = FbxNode::Create(fbxModel->manager, node->name.c_str());
		node->fbxNode->SetNodeAttribute(fbxSkel);
		std::vector<float> loc = { 0, 0, 0 };
		if (node->parentNodeIndex != -1)
		{
			// To reverse inheritance of location
			for (int i = 0; i < 3; i++)
				loc[i] = node->dost->location[i] - nodes[node->parentNodeIndex]->dost->location[i];
		}
		node->fbxNode->LclTranslation.Set(FbxDouble3(-loc[0] * 100, loc[2] * 100, loc[1] * 100));

		bones.push_back(node);
	}
	// Building heirachy
	FbxNode* root = nullptr;
	for (auto& node : nodes)
	{
		if (node->parentNodeIndex != -1)
			nodes[node->parentNodeIndex]->fbxNode->AddChild(node->fbxNode);
		else
		{
			FbxSkeleton* nodeatt = FbxSkeleton::Create(fbxModel->manager, node->name.c_str());
			nodeatt->SetSkeletonType(FbxSkeleton::eRoot);
			root = FbxNode::Create(fbxModel->manager, "root");
			root->AddChild(node->fbxNode);
			root->SetNodeAttribute(nodeatt);
		}
		if (root != nullptr)
			fbxModel->scene->GetRootNode()->AddChild(root);
	}
}

std::vector<std::vector<float_t>> Dynamic::trimVertsData(std::vector<std::vector<float_t>> verts, std::set<int> dsort, bool bVertCol)
{
	std::vector<std::vector<float_t>> newVec;
	std::vector<float_t> zeroVec = { 0, 0, 0, 0 };
	for (auto& val : dsort)
	{
		if (bVertCol)
		{
			if (val >= verts.size()) newVec.push_back(zeroVec);
			else newVec.push_back(verts[val]);
		}
		else
		{
			newVec.push_back(verts[val]);
		}
	}
	return newVec;
}

std::vector<std::vector<uint8_t>> Dynamic::trimVertsData(std::vector<std::vector<uint8_t>> verts, std::set<int> dsort)
{
	std::vector<std::vector<uint8_t>> newVec;
	for (auto& val : dsort)
	{
		newVec.push_back(verts[val]);
	}
	return newVec;
}


void Dynamic::pack(std::string saveDirectory, bool bCBuffer)
{
	std::filesystem::create_directories(saveDirectory);
	for (int i = 0; i < meshes.size(); i++)
	{
		DynamicMesh* mesh = meshes[i];
		for (int j = 0; j < mesh->submeshes.size(); j++)
		{
			DynamicSubmesh* submesh = mesh->submeshes[j];
			bool add = false;
			bool firstLodCheck = false;
			for (auto& x : mesh->submeshes)
			{
				if (x->lodLevel == 0 && x->lodGroup == submesh->lodGroup)
				{
					firstLodCheck = true;
					break;
				}
			}
			int secondLodCheck = 9999;
			for (auto& x : mesh->submeshes)
			{
				if (x->lodGroup == submesh->lodGroup)
				{
					if (x->lodLevel < secondLodCheck) secondLodCheck = x->lodLevel;
				}
			}
			if (firstLodCheck)
			{
				if (submesh->lodLevel == 0) add = true;
			}
			else if (submesh->lodLevel == secondLodCheck) add = true;
			if (add && submesh->faces.size() != 0)
			{
				if (meshes.size() == 1) submesh->name = hash + "_" + std::to_string(j);
				else submesh->name = hash + "_" + std::to_string(i) + "_" + std::to_string(j);

				FbxNode* node = fbxModel->addSubmeshToFbx(submesh, bones, h64Table, saveDirectory, bTextures, bCBuffer);
				nodes.push_back(node);
			}
		}
	}
	if (bTextures)
	{
		// Export texplates
		for (auto& texplateSet : texplateSets)
		{
			bool status = texplateSet->parse();
			if (!status) continue;
			texplateSet->saveTexturePlateSet(saveDirectory + "/textures/");
		}

		if (!externalMaterials.size()) return;
		// Export unk material textures
		std::filesystem::create_directories(saveDirectory + "/unk_textures/");
		for (auto& mat : externalMaterials)
		{
			mat->parseMaterial(h64Table);
			mat->exportTextures(saveDirectory + "/unk_textures/", "png");
		}
	}
}

void Dynamic::save(std::string saveDirectory, std::string saveName)
{
	if (saveName == "") saveName = hash;

	for (auto& node : nodes) fbxModel->scene->GetRootNode()->AddChild(node);

	fbxModel->save(saveDirectory + "/" + saveName + ".fbx", false);
}

bool Dynamic::RequestInfo(int& MeshCount, bool& bHasSkeleton)
{
	getData();
	getDyn3Files();  // Gets skeleton hash if it exists
	// Manually getting the meshcount of the first dyn3
	for (int i = 0; i < dyn3s.size(); i++)
	{
		File* dyn3 = dyn3s[i];
		uint32_t count;
		memcpy((char*)&count, dyn3->data + 0x10, 4);
		MeshCount += count;
	}
	bHasSkeleton = skeletonHash != "";

	return true;
}

bool Dynamic::RequestSaveDynamicMeshData()
{
	getData();
	getDyn3Files();
	parseDyn3s();
	getSubmeshes();

	// Then save
	FILE* meshFile;
	std::string path = "msh.tmp";
	int status = fopen_s(&meshFile, path.c_str(), "wb");
	if (status || meshFile == NULL) return false;

	for (int i = 0; i < meshes.size(); i++)
	{
		DynamicMesh* mesh = meshes[i];
		for (int j = 0; j < mesh->submeshes.size(); j++)
		{
			DynamicSubmesh* submesh = mesh->submeshes[j];
			bool add = false;
			bool firstLodCheck = false;
			for (auto& x : mesh->submeshes)
			{
				if (x->lodLevel == 0 && x->lodGroup == submesh->lodGroup)
				{
					firstLodCheck = true;
					break;
				}
			}
			int secondLodCheck = 9999;
			for (auto& x : mesh->submeshes)
			{
				if (x->lodGroup == submesh->lodGroup)
				{
					if (x->lodLevel < secondLodCheck) secondLodCheck = x->lodLevel;
				}
			}
			if (firstLodCheck)
			{
				if (submesh->lodLevel == 0) add = true;
			}
			else if (submesh->lodLevel == secondLodCheck) add = true;
			if (add && submesh->faces.size() != 0)
			{
				// Then we save this data

				// Write vertex count
				uint32_t s = submesh->vertPos.size();
				fwrite(&s, 4, 1, meshFile);

				// Write vertices
				for (auto& vertex : submesh->vertPos)
				{
					for (auto& vert : vertex)
					{
						fwrite(&vert, 1, sizeof(vert), meshFile);
					}
				}

				// Write face count
				s = submesh->faces.size();
				fwrite(&s, 4, 1, meshFile);

				// Write faces
				for (auto& face : submesh->faces)
				{
					for (auto& f : face)
					{
						fwrite(&f, 4, 1, meshFile);
					}
				}
			}
		}
	}
	fclose(meshFile);
	return true;
}
