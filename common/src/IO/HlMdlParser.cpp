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

#include "Assets/EntityModel.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/Path.h"
#include "IO/Reader.h"

#include <iomanip>
#include <sstream>

namespace TrenchBroom {
    namespace IO {
        namespace HlMdlLayout {
            static const int IdentMdl = (('T'<<24) + ('S'<<16) + ('D'<<8) + 'I');
            static const int IdentSeq = (('Q'<<24) + ('S'<<16) + ('D'<<8) + 'I');
            static const int Version10 = 10;
        }

        HlMdlParser::HlMdlParser(const std::string& name, const char* begin, const char* end, const FileSystem& fs, const std::string& extension, const std::string basePath) :
        m_name(name),
        m_begin(begin),
        m_end(end),
        m_fs(fs),
        m_extension(extension),
        m_basePath(basePath) {
            assert(m_begin < m_end);
            unused(m_end);
        }

        std::unique_ptr<Assets::EntityModel> HlMdlParser::doInitializeModel(Logger& /*logger*/) {
            auto reader = Reader::from(m_begin, m_end);
            m_file = new HlMdlFile();
            parseFile(reader);

            // If we don't have textures in this MDL file, then they are externalized and located within the "T" MDL file
            if (m_file->header.numtextures <= 0) {
                loadExternalTexturesModelFile();
            }

            // Load extra sequences MDL files (i.e. "01") if we need to
            if (m_file->header.numseqgroups > 1) {
                loadExternalSequencesModelFiles();
            }

            // TODO: fill "model" with data
            // TODO: check if pitch type is correct
            auto model = std::make_unique<Assets::EntityModel>(m_name, Assets::PitchType::Normal);
            return model;
        }

        void HlMdlParser::loadExternalSequencesModelFiles() {
            for (size_t i = 1; i < m_file->header.numseqgroups; ++i) {
                std::stringstream seqNum;
                seqNum << std::setw(2) << std::setfill('0') << i;

                std::string fileName = m_basePath.substr(0, m_basePath.rfind('.'));
                fileName.append(seqNum.str());
                fileName.append(".");
                fileName.append(m_extension);

                const auto file = m_fs.openFile(Path(fileName));
                if (file == nullptr) {
                    throw AssetException("Failed to read sequences model file");
                }

                auto reader = file->reader().buffer();
                HlMdlSequenceHeader seqHeader = parseSequenceHeader(reader);
                m_sequencesFiles.emplace_back(seqHeader);
            }
        }

        void HlMdlParser::loadExternalTexturesModelFile() {
            std::string fileName = m_basePath.substr(0, m_basePath.rfind('.'));
            fileName.append("t.");
            fileName.append(m_extension);

            const auto file = m_fs.openFile(Path(fileName));
            if (file == nullptr) {
                throw AssetException("Failed to read external textures model file");
            }

            auto reader = file->reader().buffer();
            m_textureFile = new HlMdlFile();
            parseExternalTextureFile(reader);
        }

        void HlMdlParser::parseFile(Reader& reader) {
            m_file->header = parseModelHeader(reader);

            reader.seekFromBegin(static_cast<size_t>(m_file->header.boneindex));
            for (size_t i = 0; i < m_file->header.numbones; ++i) {
                m_file->bones.emplace_back(parseBone(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.bonecontrollerindex));
            for (size_t i = 0; i < m_file->header.numbonecontrollers; ++i) {
                m_file->boneControllers.emplace_back(parseBoneController(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.hitboxindex));
            for (size_t i = 0; i < m_file->header.numhitboxes; ++i) {
                m_file->hitBoxes.emplace_back(parseHitBox(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.seqindex));
            for (size_t i = 0; i < m_file->header.numseq; ++i) {
                m_file->sequences.emplace_back(parseSequence(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.seqgroupindex));
            for (size_t i = 0; i < m_file->header.numseqgroups; ++i) {
                m_file->sequenceGroups.emplace_back(parseSequenceGroup(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.textureindex));
            for (size_t i = 0; i < m_file->header.numtextures; ++i) {
                m_file->textures.emplace_back(parseTexture(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.skinindex));
            for (size_t i = 0; i < m_file->header.numskinref; ++i) {
                m_file->skins.emplace_back(reader.readShort<short>());
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.bodypartindex));
            for (size_t i = 0; i < m_file->header.numbodyparts; ++i) {
                m_file->bodyParts.emplace_back(parseBodyParts(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.attachmentindex));
            for (size_t i = 0; i < m_file->header.numattachments; ++i) {
                m_file->attachments.emplace_back(parseAttachment(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_file->header.transitionindex));
            for (size_t i = 0; i < m_file->header.numtransitions; ++i) {
                m_file->transitions.emplace_back(reader.readUnsignedChar<unsigned char>());
            }
        }
 
        void HlMdlParser::parseExternalTextureFile(Reader& reader) {
            m_textureFile->header = parseModelHeader(reader);

            reader.seekFromBegin(static_cast<size_t>(m_textureFile->header.textureindex));
            for (size_t i = 0; i < m_textureFile->header.numtextures; ++i) {
                m_textureFile->textures.emplace_back(parseTexture(reader));
            }

            reader.seekFromBegin(static_cast<size_t>(m_textureFile->header.skinindex));
            for (size_t i = 0; i < m_textureFile->header.numskinref; ++i) {
                m_textureFile->skins.emplace_back(reader.readShort<short>());
            }
        }

        HlMdlParser::HlMdlModelHeader HlMdlParser::parseModelHeader(Reader& reader) {
            HlMdlModelHeader header;

            header.id = reader.readInt<int32_t>();
            if (header.id == HlMdlLayout::IdentSeq) {
                throw AssetException("Illegal attempt to load a HL sequence model as a HL model");
            }
            if (header.id != HlMdlLayout::IdentMdl) {
                throw AssetException("Unknown HLMDL model ident: " + std::to_string(header.id));
            }

            header.version = reader.readInt<int32_t>();
            if (header.version != HlMdlLayout::Version10) {
                throw AssetException("Unknown HLMDL model version: " + std::to_string(header.version));
            }

            header.name = reader.readString(HlMdlConstants::HeaderNameSize);
            header.length = reader.readInt<int32_t>();

            header.eyeposition = reader.readVec<float, 3>();
            header.min = reader.readVec<float, 3>();
            header.max = reader.readVec<float, 3>();

            header.bbmin = reader.readVec<float, 3>();
            header.bbmax = reader.readVec<float, 3>();

            header.flags = reader.readInt<int32_t>();

            header.numbones = reader.readInt<int32_t>();
            header.boneindex = reader.readInt<int32_t>();

            header.numbonecontrollers = reader.readInt<int32_t>();
            header.bonecontrollerindex = reader.readInt<int32_t>();

            header.numhitboxes = reader.readInt<int32_t>();
            header.hitboxindex = reader.readInt<int32_t>();

            header.numseq = reader.readInt<int32_t>();
            header.seqindex = reader.readInt<int32_t>();

            header.numseqgroups = reader.readInt<int32_t>();
            header.seqgroupindex = reader.readInt<int32_t>();

            header.numtextures = reader.readInt<int32_t>();
            header.textureindex = reader.readInt<int32_t>();
            header.texturedataindex = reader.readInt<int32_t>();

            header.numskinref = reader.readInt<int32_t>();
            header.numskinfamilies = reader.readInt<int32_t>();
            header.skinindex = reader.readInt<int32_t>();

            header.numbodyparts = reader.readInt<int32_t>();
            header.bodypartindex = reader.readInt<int32_t>();

            header.numattachments = reader.readInt<int32_t>();
            header.attachmentindex = reader.readInt<int32_t>();

            header.soundtable = reader.readInt<int32_t>();
            header.soundindex = reader.readInt<int32_t>();
            header.soundgroups = reader.readInt<int32_t>();
            header.soundgroupindex = reader.readInt<int32_t>();

            header.numtransitions = reader.readInt<int32_t>();
            header.transitionindex = reader.readInt<int32_t>();

            return header;
        }

        HlMdlParser::HlMdlSequenceHeader HlMdlParser::parseSequenceHeader(Reader& reader) {
            HlMdlSequenceHeader header;

            header.id = reader.readInt<int32_t>();
            if (header.id == HlMdlLayout::IdentMdl) {
                throw AssetException("Illegal attempt to load a HL model as a HL sequence model");
            }
            if (header.id != HlMdlLayout::IdentSeq) {
                throw AssetException("Unknown HLMDL sequence model ident: " + std::to_string(header.id));
            }

            header.version = reader.readInt<int32_t>();
            if (header.version != HlMdlLayout::Version10) {
                throw AssetException("Unknown HLMDL model version: " + std::to_string(header.version));
            }

            header.name = reader.readString(HlMdlConstants::HeaderNameSize);
            header.length = reader.readInt<int32_t>();

            return header;
        }

        HlMdlParser::HlMdlBone HlMdlParser::parseBone(Reader& reader) {
            HlMdlBone bone;

            bone.name = reader.readString(HlMdlConstants::BoneNameSize);
            bone.parent = reader.readInt<int32_t>();
            bone.flags = reader.readInt<int32_t>();

            for (size_t i = 0; i < HlMdlConstants::MaxControllersPerBone; ++i) {
                bone.bonecontroller[i] = reader.readInt<int32_t>();
            }

            for (size_t i = 0; i < HlMdlConstants::MaxControllersPerBone; ++i) {
                bone.value[i] = reader.readFloat<float>();
            }

            for (size_t i = 0; i < HlMdlConstants::MaxControllersPerBone; ++i) {
                bone.scale[i] = reader.readFloat<float>();
            }

            return bone;
        }

        HlMdlParser::HlMdlBoneController HlMdlParser::parseBoneController(Reader& reader) {
            HlMdlBoneController boneController;

            boneController.bone = reader.readInt<int32_t>();
            boneController.type = reader.readInt<int32_t>();
            boneController.start = reader.readFloat<float>();
            boneController.end = reader.readFloat<float>();
            boneController.rest = reader.readInt<int32_t>();
            boneController.index = reader.readInt<int32_t>();

            return boneController;
        }

        HlMdlParser::HlMdlHitBox HlMdlParser::parseHitBox(Reader& reader) {
            HlMdlHitBox hbox;

            hbox.bone = reader.readInt<int32_t>();
            hbox.group = reader.readInt<int32_t>();
            hbox.bbmin = reader.readVec<float, 3>();
            hbox.bbmax = reader.readVec<float, 3>();

            return hbox;
        }

        HlMdlParser::HlMdlSequence HlMdlParser::parseSequence(Reader& reader) {
            HlMdlSequence seq;

            seq.label = reader.readString(HlMdlConstants::SequenceLabelSize);

            seq.fps = reader.readFloat<float>();
            seq.flags = reader.readInt<int32_t>();

            seq.activity = reader.readInt<int32_t>();
            seq.actweight = reader.readInt<int32_t>();

            seq.numevents = reader.readInt<int32_t>();
            seq.eventindex = reader.readInt<int32_t>();

            seq.numframes = reader.readInt<int32_t>();

            seq.numpivots = reader.readInt<int32_t>();
            seq.pivotindex = reader.readInt<int32_t>();

            seq.motiontype = reader.readInt<int32_t>();
            seq.motionbone = reader.readInt<int32_t>();
            seq.linearmovement = reader.readVec<float, 3>();
            seq.automoveposindex = reader.readInt<int32_t>();
            seq.automovenangleindex = reader.readInt<int32_t>();

            seq.bbmin = reader.readVec<float, 3>();
            seq.bbmax = reader.readVec<float, 3>();

            seq.numblends = reader.readInt<int32_t>();
            seq.animindex = reader.readInt<int32_t>();

            for (size_t i = 0; i < HlMdlConstants::SequenceBlendSize; ++i) {
                seq.blendtype[i] = reader.readInt<int32_t>();
            }
            for (size_t i = 0; i < HlMdlConstants::SequenceBlendSize; ++i) {
                seq.blendstart[i] = reader.readFloat<float>();
            }
            for (size_t i = 0; i < HlMdlConstants::SequenceBlendSize; ++i) {
                seq.blendend[i] = reader.readFloat<float>();
            }
            seq.blendparent = reader.readInt<int32_t>();

            seq.seqgroup = reader.readInt<int32_t>();

            seq.entrynode = reader.readInt<int32_t>();
            seq.exitnode = reader.readInt<int32_t>();
            seq.nodeflags = reader.readInt<int32_t>();

            seq.nextseq = reader.readInt<int32_t>();

            return seq;
        }

        HlMdlParser::HlMdlSequenceGroup HlMdlParser::parseSequenceGroup(Reader& reader) {
            HlMdlSequenceGroup seqGroup;

            seqGroup.label = reader.readString(HlMdlConstants::SequenceGroupLabelSize);
            seqGroup.name = reader.readString(HlMdlConstants::SequenceGroupNameSize);
            seqGroup.unused1 = reader.readInt<int32_t>();
            seqGroup.unused2 = reader.readInt<int32_t>();

            return seqGroup;
        }

        HlMdlParser::HlMdlTexture HlMdlParser::parseTexture(Reader& reader) {
            HlMdlTexture tex;

            tex.name = reader.readString(HlMdlConstants::TextureNameSize);
            tex.flags = reader.readInt<int32_t>();
            tex.width = reader.readInt<int32_t>();
            tex.height = reader.readInt<int32_t>();
            tex.index = reader.readInt<int32_t>();

            return tex;
        }

        HlMdlParser::HlMdlBodyParts HlMdlParser::parseBodyParts(Reader& reader) {
            HlMdlBodyParts bodyParts;

            bodyParts.name = reader.readString(HlMdlConstants::BodyPartsNameSize);
            bodyParts.nummodels = reader.readInt<int32_t>();
            bodyParts.base = reader.readInt<int32_t>();
            bodyParts.modelindex = reader.readInt<int32_t>();

            return bodyParts;
        }

        HlMdlParser::HlMdlAttachment HlMdlParser::parseAttachment(Reader& reader) {
            HlMdlAttachment attachment;

            attachment.name = reader.readString(HlMdlConstants::AttachmentNameSize);
            attachment.type = reader.readInt<int32_t>();
            attachment.bone = reader.readInt<int32_t>();
            attachment.org = reader.readVec<float, 3>();
            for (size_t i = 0; i < HlMdlConstants::AttachmentVectorsSize; ++i) {
                attachment.vectors[i] = reader.readVec<float, 3>();
            }

            return attachment;
        }
    }
}
