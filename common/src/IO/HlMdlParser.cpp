/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "HlMdlParser.h"

#include "Exceptions.h"
#include "Assets/EntityModel.h"
#include "IO/Reader.h"
#include "IO/File.h"
#include "IO/FileSystem.h"

#include <string>
#include <vector>

namespace TrenchBroom {
    namespace IO {
        namespace HlMdlLayout {
            static const int IdentMdl = (('T'<<24) + ('S'<<16) + ('D'<<8) + 'I');
            static const int IdentSeq = (('Q'<<24) + ('S'<<16) + ('D'<<8) + 'I');
            static const int Version10 = 10;
        }

        HlMdlParser::HlMdlParser(const std::string& name, const char* begin, const char* end, const std::string& extension, const FileSystem& fs) :
        m_name(name),
        m_begin(begin),
        m_end(end),
        m_extension(extension),
        m_fs(fs) {
            assert(m_begin < m_end);
            unused(m_end);
        }

        std::unique_ptr<Assets::EntityModel> HlMdlParser::doInitializeModel(Logger& /* logger */) {
            auto reader = Reader::from(m_begin, m_end);
            m_modelHeader = parseHeader(reader, false);

            const auto baseName = m_name.substr(0, m_name.length() - 4);

            // If we don't have textures in this MDL file, then the textures are externalized and located in the "<modelname>T.mdl" file.
            if (m_modelHeader.numtextures == 0) {
                loadExternalTexturesModelFile(baseName);
            } else {
                m_textureHeader = m_modelHeader;
            }

            // Load the extra model sequences file(s) like "<modelname>01.mdl" if we need to.
            if (m_modelHeader.numseqgroups > 1) {
                loadSequenceModelFiles(baseName);
            }

            // TODO - Fill "model" with "surface(s) data" like mesh(es), skin(s)...
            // TODO - Check if pitch type is correct, Half-Life seems to be using a "right hand grip rule" where
            // X = Roll
            // Y = Pitch
            // Z = Yaw
            auto model = std::make_unique<Assets::EntityModel>(m_name, Assets::PitchType::Normal);
            model->addFrames(getTotalFramesCount(reader));

            return model;
        }

        void HlMdlParser::doLoadFrame(const size_t frameIndex, Assets::EntityModel& model, Logger& /* logger */) {
            auto reader = Reader::from(m_begin, m_end);
            m_modelHeader = parseHeader(reader, false);

            const auto baseName = m_name.substr(0, m_name.length() - 4);

            // If we don't have textures in this MDL file, then the textures are externalized and located in the "<modelname>T.mdl" file.
            if (m_modelHeader.numtextures == 0) {
                loadExternalTexturesModelFile(baseName);
            } else {
                m_textureHeader = m_modelHeader;
            }

            // Load the extra model sequences file(s) like "<modelname>01.mdl" if we need to.
            if (m_modelHeader.numseqgroups > 1) {
                loadSequenceModelFiles(baseName);
            }
        }

        HlMdlParser::HlMdlBodyParts HlMdlParser::getBodyPartById(Reader &reader, unsigned int id) {
            if (id >= m_modelHeader.numbodyparts) {
                id = m_modelHeader.numbodyparts - 1;
            }

            reader.seekFromBegin(static_cast<size_t>(m_modelHeader.bodypartindex) + id);
            return parseBodyParts(reader);
        }

        size_t HlMdlParser::getTotalFramesCount(Reader& reader) {
            size_t result = 0;
            reader.seekFromBegin(static_cast<size_t>(m_modelHeader.seqindex));

            for (size_t seq = 0; seq < m_modelHeader.numseq; seq++) {
                const HlMdlSequenceDescription seqDesc = parseSequenceDescription(reader);
                result += seqDesc.numframes;
            }

            return result;
        }

        void HlMdlParser::loadExternalTexturesModelFile(const std::string& baseFileName) {
            auto texMdlName = baseFileName;
            texMdlName.append("t.");
            texMdlName.append(m_extension);

#ifdef WIN32 // HACK - Replace this crap by a proper "get working directory" solution
            const auto texMdlFile = m_fs.openFile(Path("models\\" + texMdlName));
#else
            const auto texMdlFile = m_fs.openFile(Path("models/" + texMdlName));
#endif
            if (texMdlFile == nullptr) {
                throw AssetException("Can't load HLMDL texture model: " + texMdlName);
            }

            // TODO - There is probably a better way to do this without "buf"
            const auto buf = texMdlFile->reader().buffer();
            auto texReader = Reader::from(std::begin(buf), std::end(buf));

            m_textureHeader = parseHeader(texReader, false);
        }

        void HlMdlParser::loadSequenceModelFiles(const std::string& baseFileName) {
            for (size_t i = 1; i < m_modelHeader.numseqgroups; ++i) {
                char seqId[3];
                snprintf(seqId, sizeof seqId, "%02d", i);

                auto seqMdlName = baseFileName;
                seqMdlName.append(seqId);
                seqMdlName.append("." + m_extension);

#ifdef WIN32 // HACK - Replace this crap by a proper "get working directory" solution
                const auto seqMdlFile = m_fs.openFile(Path("models\\" + seqMdlName));
#else
                const auto seqMdlFile = m_fs.openFile(Path("models/" + seqMdlName));
#endif
                if (seqMdlFile == nullptr) {
                    throw AssetException("Can't load HLMDL sequence model: " + seqMdlName);
                }

                // TODO - There is probably a better way to do this without "buf"
                const auto buf = seqMdlFile->reader().buffer();
                auto seqReader = Reader::from(std::begin(buf), std::end(buf));

                const auto seqHeader = parseHeader(seqReader, true);
                m_sequenceHeaders.emplace_back(seqHeader);
            }
        }

        HlMdlParser::HlMdlHeader HlMdlParser::parseHeader(Reader& reader, const bool sequenceFile) {
            HlMdlHeader header;

            header.id = reader.readInt<int32_t>();
            if (header.id != HlMdlLayout::IdentMdl && header.id != HlMdlLayout::IdentSeq) {
                throw AssetException("Unknown HLMDL model ident: " + std::to_string(header.id));
            }

            if (!sequenceFile && header.id == HlMdlLayout::IdentSeq) {
                throw AssetException("Illegal attempt to load a HL sequence model as a HL model");
            }
            if (sequenceFile && header.id == HlMdlLayout::IdentMdl) {
                throw AssetException("Illegal attempt to load a HL model as a HL sequence model");
            }

            header.version = reader.readInt<int32_t>();
            if (header.version != HlMdlLayout::Version10) {
                throw AssetException("Unknown HLMDL model version: " + std::to_string(header.version));
            }

            // TODO - There is likely no need to fill the entire structure.
            header.name = reader.readString(HlMdlConstants::HeaderNameSize);
            header.length = reader.readInt<int32_t>();

            header.eyeposition = reader.readVec<float, 3>();
            header.min = reader.readVec<float, 3>();
            header.max = reader.readVec<float, 3>();

            header.bbmin = reader.readVec<float, 3>();
            header.bbmax = reader.readVec<float, 3>();

            header.flags = reader.readInt<int32_t>();

            header.numbones = reader.readSize<size_t>();
            header.boneindex = reader.readInt<int32_t>();

            header.numbonecontrollers = reader.readSize<size_t>();
            header.bonecontrollerindex = reader.readInt<int32_t>();

            header.numhitboxes = reader.readSize<size_t>();
            header.hitboxindex = reader.readInt<int32_t>();

            header.numseq = reader.readSize<size_t>();
            header.seqindex = reader.readInt<int32_t>();

            header.numseqgroups = reader.readSize<size_t>();
            header.seqgroupindex = reader.readInt<int32_t>();

            header.numtextures = reader.readSize<size_t>();
            header.textureindex = reader.readInt<int32_t>();
            header.texturedataindex = reader.readInt<int32_t>();

            return header;
        }

        HlMdlParser::HlMdlBone HlMdlParser::parseBone(Reader& reader) {
            HlMdlBone bone;

            bone.name = reader.readString(HlMdlConstants::BoneNameSize);
            bone.parent = reader.readInt<int32_t>();
            bone.flags = reader.readInt<int32_t>();
            for (size_t i = 0; i < HlMdlConstants::BoneMaxPerBoneControllers; i++) {
                bone.bonecontroller[i] = reader.readInt<int32_t>();
            }
            for (size_t i = 0; i < HlMdlConstants::BoneMaxPerBoneControllers; i++) {
                bone.value[i] = reader.readFloat<float>();
            }
            for (size_t i = 0; i < HlMdlConstants::BoneMaxPerBoneControllers; i++) {
                bone.scale[i] = reader.readFloat<float>();
            }

            return bone;
        }

        HlMdlParser::HlMdlSequenceDescription HlMdlParser::parseSequenceDescription(Reader& reader) {
            HlMdlSequenceDescription seqDesc;

            // TODO - There is likely no need to fill the entire structure.
            seqDesc.label = reader.readString(HlMdlConstants::SequenceDescriptionSequenceLabelSize);

            seqDesc.fps = reader.readFloat<float>();
            seqDesc.flags = reader.readInt<int32_t>();

            seqDesc.activity = reader.readInt<int32_t>();
            seqDesc.actweight = reader.readInt<int32_t>();

            seqDesc.numevents = reader.readSize<size_t>();
            seqDesc.eventindex = reader.readInt<int32_t>();

            seqDesc.numframes = reader.readSize<size_t>();

            seqDesc.numpivots = reader.readSize<size_t>();
            seqDesc.pivotindex = reader.readInt<int32_t>();

            seqDesc.motiontype = reader.readInt<int32_t>();
            seqDesc.motionbone = reader.readInt<int32_t>();
            seqDesc.linearmovement = reader.readVec<float, 3>();
            seqDesc.automoveposindex = reader.readInt<int32_t>();
            seqDesc.automoveangleindex = reader.readInt<int32_t>();

            seqDesc.bbmin = reader.readVec<float, 3>();
            seqDesc.bbmax = reader.readVec<float, 3>();

            seqDesc.numblends = reader.readSize<size_t>();
            seqDesc.animindex = reader.readInt<int32_t>();

            for (size_t i = 0; i < HlMdlConstants::SequenceDescriptionMaxNumBlendTypes; i++) {
                seqDesc.blendtype[i] = reader.readInt<int32_t>();
            }
            for (size_t i = 0; i < HlMdlConstants::SequenceDescriptionMaxNumBlendTypes; i++) {
                seqDesc.blendstart[i] = reader.readFloat<float>();
            }
            for (size_t i = 0; i < HlMdlConstants::SequenceDescriptionMaxNumBlendTypes; i++) {
                seqDesc.blendend[i] = reader.readFloat<float>();
            }
            seqDesc.blendparent = reader.readInt<int32_t>();

            seqDesc.seqgroup = reader.readInt<int32_t>();

            seqDesc.entrynode = reader.readInt<int32_t>();
            seqDesc.exitnode = reader.readInt<int32_t>();
            seqDesc.nodeflags = reader.readInt<int32_t>();

            seqDesc.nextseq = reader.readInt<int32_t>();

            return seqDesc;
        }

        HlMdlParser::HlMdlModel HlMdlParser::parseModel(Reader& reader) {
            HlMdlModel model;

            // TODO - There is likely no need to fill the entire structure.
            model.name = reader.readString(HlMdlConstants::ModelNameSize);

            model.type = reader.readInt<int32_t>();

            model.boundingradius = reader.readFloat<float>();

            model.nummesh = reader.readSize<size_t>();
            model.meshindex = reader.readInt<int32_t>();

            model.numverts = reader.readSize<size_t>();
            model.vertinfoindex = reader.readInt<int32_t>();
            model.vertindex = reader.readInt<int32_t>();
            model.numnorms = reader.readSize<size_t>();
            model.norminfoindex = reader.readInt<int32_t>();
            model.normindex = reader.readInt<int32_t>();

            model.numgroups = reader.readSize<size_t>();
            model.groupindex = reader.readInt<int32_t>();

            return model;
        }

        HlMdlParser::HlMdlBodyParts HlMdlParser::parseBodyParts(Reader& reader) {
            HlMdlBodyParts bodyParts;

            // TODO - There is likely no need to fill the entire structure.
            bodyParts.name = reader.readString(HlMdlConstants::BodyPartsNameSize);
            bodyParts.nummodels = reader.readSize<size_t>();
            bodyParts.base = reader.readInt<int32_t>();
            bodyParts.modelindex = reader.readInt<int32_t>();

            return bodyParts;
        }
    }
}
