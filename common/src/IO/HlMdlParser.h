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

#ifndef TrenchBroom_HlMdlParser
#define TrenchBroom_HlMdlParser

#include "Assets/EntityModel_Forward.h"
#include "IO/EntityModelParser.h"

#include <string>
#include <vector>

namespace TrenchBroom {
    namespace IO {
        class FileSystem;
        class Reader;

        namespace HlMdlConstants {
            // Model/Sequence header
            static const int HeaderNameSize = 64;

            // Bone
            static const int BoneNameSize = 32;
            static const int MaxControllersPerBone = 6;

            // Sequence
            static const int SequenceLabelSize = 32;
            static const int SequenceBlendSize = 2;

            // Sequence group
            static const int SequenceGroupLabelSize = 32;
            static const int SequenceGroupNameSize = 64;

            // Texture
            static const int TextureNameSize = 64;

            // Body parts
            static const int BodyPartsNameSize = 64;

            // Attachment
            static const int AttachmentNameSize = 32;
            static const int AttachmentVectorsSize = 3;
        }

        class HlMdlParser : public EntityModelParser {
        private:
            // HL SDK name: "studiohdr_t"
            struct HlMdlModelHeader {
                int32_t id;
                int32_t version;

                std::string name;
                int32_t length;

                vm::vec3f eyeposition; // ideal eye position
                vm::vec3f min; // ideal movement hull size
                vm::vec3f max;

                vm::vec3f bbmin; // clipping bounding box
                vm::vec3f bbmax;

                int32_t flags;

                int32_t numbones; // bones
                int32_t boneindex;

                int32_t numbonecontrollers; // bone controllers
                int32_t bonecontrollerindex;

                int32_t numhitboxes; // complex bounding boxes
                int32_t hitboxindex;

                int32_t numseq; // animation sequences
                int32_t seqindex;

                int32_t numseqgroups; // demand loaded sequences
                int32_t seqgroupindex;

                int32_t numtextures; // raw textures
                int32_t textureindex;
                int32_t texturedataindex;

                int32_t numskinref; // replaceable textures
                int32_t numskinfamilies;
                int32_t skinindex;

                int32_t numbodyparts;
                int32_t bodypartindex;

                int32_t numattachments; // queryable attachable points
                int32_t attachmentindex;

                // This seems to be obsolete. Probably replaced by events that reference external sounds?
                int32_t soundtable;
                int32_t soundindex;
                int32_t soundgroups;
                int32_t soundgroupindex;

                int32_t numtransitions; // animation node to animation node transition graph
                int32_t transitionindex;
            };

            // HL SDK name: "studioseqhdr_t"
            struct HlMdlSequenceHeader {
                int32_t id;
                int32_t version;

                std::string name;
                int32_t length;
            };

            // HL SDK name: "mstudiobone_t"
            struct HlMdlBone {
                std::string name; // bone name for symbolic links
                int32_t parent; // parent bone
                int32_t flags; // ??
                int32_t bonecontroller[HlMdlConstants::MaxControllersPerBone]; // bone controller index, -1 == none
                float value[HlMdlConstants::MaxControllersPerBone]; // default DoF values
                float scale[HlMdlConstants::MaxControllersPerBone]; // scale for delta DoF values
            };

            using HlMdlBoneList = std::vector<HlMdlBone>;

            // HL SDK name: "mstudiobonecontroller_t"
            struct HlMdlBoneController {
                int32_t bone; // -1 == 0
                int32_t type; // X, Y, Z, XR, YR, ZR, H
                float start;
                float end;
                int32_t rest; // byte index value at rest
                int32_t index; // 0-3 user set controller, 4 mouth
            };

            using HlMdlBoneControllerList = std::vector<HlMdlBoneController>;

            // HL SDK name: "mstudiobbox_t"
            struct HlMdlHitBox {
                int32_t bone;
                int32_t group; // intersection group
                vm::vec3f bbmin; // bounding box
                vm::vec3f bbmax;
            };

            using HlMdlHitBoxList = std::vector<HlMdlHitBox>;

            // HL SDK name: "mstudioseqdesc_t"
            struct HlMdlSequence {
                std::string label; // sequence label

                float fps; // frames per seconds
                int32_t flags; // looping/non-looping flags

                int32_t activity;
                int32_t actweight;

                int32_t numevents;
                int32_t eventindex;

                int32_t numframes; // number of frames per sequence

                int32_t numpivots; // number of foot pivots
                int32_t pivotindex;

                int32_t motiontype;
                int32_t motionbone;
                vm::vec3f linearmovement;
                int32_t automoveposindex;
                int32_t automovenangleindex;

                vm::vec3f bbmin; // per sequence bounding box
                vm::vec3f bbmax;

                int32_t numblends;
                int32_t animindex; // mstudioanim_t pointer relative to start of sequence group data
                                   // [blend][bone][X, Y, Z, XR, YR, ZR]

                int32_t blendtype[HlMdlConstants::SequenceBlendSize]; // X, Y, Z, XR, YR, ZR
                float blendstart[HlMdlConstants::SequenceBlendSize];
                float blendend[HlMdlConstants::SequenceBlendSize];
                int32_t blendparent;

                int32_t seqgroup; // sequence group for demand loading

                int32_t entrynode; // transition node at entry
                int32_t exitnode; // transition node at exit
                int32_t nodeflags; // transition rules

                int32_t nextseq; // auto advancing sequences
            };

            using HlMdlSequenceList = std::vector<HlMdlSequence>;

            // HL SDK name: "mstudioseqgroup_t"
            struct HlMdlSequenceGroup {
                std::string label; // textual name
                std::string name; // file name
                int32_t unused1; // was "cache" - index pointer
                int32_t unused2; // was "data" - hack for group 0
            };

            using HlMdlSequenceGroupList = std::vector<HlMdlSequenceGroup>;

            // HL SDK name: "mstudiotexture_t"
            struct HlMdlTexture {
                std::string name;
                int32_t flags;
                int32_t width;
                int32_t height;
                int32_t index;
            };

            using HlMdlTextureList = std::vector<HlMdlTexture>;

            using HlMdlSkinList = std::vector<short>;

            // HL SDK name: "mstudiobodyparts_t"
            struct HlMdlBodyParts {
                std::string name;
                int32_t nummodels;
                int32_t base;
                int32_t modelindex; // index into models array
            };

            using HlMdlBodyPartsList = std::vector<HlMdlBodyParts>;

            // HL SDK name: "mstudioattachment_t"
            struct HlMdlAttachment {
                std::string name;
                int32_t type;
                int32_t bone;
                vm::vec3f org; // attachment point
                vm::vec3f vectors[HlMdlConstants::AttachmentVectorsSize];
            };

            using HlMdlAttachmentList = std::vector<HlMdlAttachment>;

            using HlMdlTransitionList = std::vector<unsigned char>; // In HL SDK, this is a "byte" ("typedef unsigned char byte" to be exact)

            struct HlMdlFile {
                HlMdlModelHeader header;
                HlMdlBoneList bones;
                HlMdlBoneControllerList boneControllers;
                HlMdlHitBoxList hitBoxes;
                HlMdlSequenceList sequences;
                HlMdlSequenceGroupList sequenceGroups;
                HlMdlTextureList textures;
                HlMdlSkinList skins;
                HlMdlBodyPartsList bodyParts;
                HlMdlAttachmentList attachments;
                HlMdlTransitionList transitions;
            };

            using HlMdlSequenceHeaderList = std::vector<HlMdlSequenceHeader>;

            HlMdlFile* m_file;
            HlMdlFile* m_textureFile;
            HlMdlSequenceHeaderList m_sequencesFiles;

            const std::string m_name;
            const char* m_begin;
            const char* m_end;
            const FileSystem& m_fs;
            const std::string m_extension;
            const std::string m_basePath;
        public:
            HlMdlParser(const std::string& name, const char* begin, const char* end, const FileSystem& fs, const std::string& extension, const std::string basePath);
        private:
            std::unique_ptr<Assets::EntityModel> HlMdlParser::doInitializeModel(Logger& logger);

            void loadExternalSequencesModelFiles();
            void loadExternalTexturesModelFile();
            void parseFile(Reader& reader);
            void parseExternalTextureFile(Reader& reader);

            HlMdlModelHeader parseModelHeader(Reader& reader);
            HlMdlSequenceHeader parseSequenceHeader(Reader& reader);
            HlMdlBone parseBone(Reader& reader);
            HlMdlBoneController parseBoneController(Reader& reader);
            HlMdlHitBox parseHitBox(Reader& reader);
            HlMdlSequence parseSequence(Reader& reader);
            HlMdlSequenceGroup parseSequenceGroup(Reader& reader);
            HlMdlTexture parseTexture(Reader& reader);
            HlMdlBodyParts parseBodyParts(Reader& reader);
            HlMdlAttachment parseAttachment(Reader& reader);
        };
    }
}

#endif /* defined(TrenchBroom_HlMdlParser) */
