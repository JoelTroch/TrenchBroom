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

#include <vecmath/forward.h>
#include <vecmath/vec.h>

#include <string>
#include <vector>

namespace TrenchBroom {
    namespace IO {
        namespace HlMdlConstants {
            static const int HeaderNameSize = 64;

            static const int BoneNameSize = 32;
            static const int BoneMaxPerBoneControllers = 6;

            static const int SequenceDescriptionSequenceLabelSize = 32;
            static const int SequenceDescriptionMaxNumBlendTypes = 2;

            static const int ModelNameSize = 64;

            static const int BodyPartsNameSize = 64;
        }
        class FileSystem;
        class Reader;

        class HlMdlParser : public EntityModelParser {
        private:
            struct HlMdlHeader { // This is the "studiohdr_t" structure
                int32_t id;
                int32_t version;

                std::string name; // Originally char[64]
                int32_t length;

                vm::vec3f eyeposition; // ideal eye position
                vm::vec3f min; // ideal movement hull size
                vm::vec3f max;

                vm::vec3f bbmin; // clipping bounding box
                vm::vec3f bbmax;

                int32_t flags;

                size_t numbones; // bones
                int32_t boneindex;

                size_t numbonecontrollers; // bone controllers
                int32_t bonecontrollerindex;

                size_t numhitboxes; // complex bounding boxes
                int32_t hitboxindex;

                size_t numseq; // animation sequences
                int32_t seqindex;

                size_t numseqgroups; // demand loaded sequences
                int32_t seqgroupindex;

                size_t numtextures; // raw textures
                int32_t textureindex;
                int32_t texturedataindex;

                size_t numskinref; // replaceable textures
                size_t numskinfamilies;
                int32_t skinindex;

                size_t numbodyparts;
                int32_t bodypartindex;

                size_t numattachments; // queryable attachable points
                int32_t attachmentindex;

                // This seems to be obsolete. Probably replaced by events that reference external sounds?
                int32_t soundtable;
                int32_t soundindex;
                int32_t soundgroups;
                int32_t soundgroupindex;

                size_t numtransitions; // animation node to animation node transition graph
                int32_t transitionindex;
            };

            using HlMdlSequenceHeaderList = std::vector<HlMdlHeader>;

            struct HlMdlBone {
                std::string name; // bone name for symbolic links - Originally char[32]
                int32_t parent; // parent bone
                int32_t flags; // ??
                int32_t bonecontroller[HlMdlConstants::BoneMaxPerBoneControllers]; // bone controller index, -1 == none
                float value[HlMdlConstants::BoneMaxPerBoneControllers]; // default DoF values
                float scale[HlMdlConstants::BoneMaxPerBoneControllers]; // scale for delta DoF values
            };

            struct HlMdlSequenceDescription { // This is the "mstudioseqdesc_t" structure
                std::string label; // sequence label - Originally char[32]

                float fps; // frames per second
                int32_t flags; // looping/non-looping flags

                int32_t activity;
                int32_t actweight;

                size_t numevents;
                int32_t eventindex;

                size_t numframes; // number of frames per sequence

                size_t numpivots; // number of foot pivots
                int32_t pivotindex;

                int32_t motiontype;
                int32_t motionbone;
                vm::vec3f linearmovement;
                int32_t automoveposindex;
                int32_t automoveangleindex;

                vm::vec3f bbmin; // per sequence bounding box
                vm::vec3f bbmax;

                size_t numblends;
                int32_t animindex; // mstudioanim_t pointer relative to start of sequence group data
                                   // [blend][bone][X, Y, Z, XR, YR, ZR]

                int32_t blendtype[HlMdlConstants::SequenceDescriptionMaxNumBlendTypes]; // X, Y, Z, XR, YR, ZR
                float blendstart[HlMdlConstants::SequenceDescriptionMaxNumBlendTypes]; // starting value
                float blendend[HlMdlConstants::SequenceDescriptionMaxNumBlendTypes]; // ending value
                int32_t blendparent;

                int32_t seqgroup; // sequence group for demand loading

                int32_t entrynode; // transition node at entry
                int32_t exitnode; // transition node at exit
                int32_t nodeflags; // transition rules

                int32_t nextseq; // auto advancing sequences
            };

            struct HlMdlModel { // This is the "mstudiomodel_t" structure
                std::string name; // Originally char[64]

                int32_t type;

                float boundingradius;

                size_t nummesh;
                int32_t meshindex;

                size_t numverts; // number of unique vertices
                int32_t vertinfoindex; // vertex bone info
                int32_t vertindex; // vertex vec3
                size_t numnorms; // number of unique surface normals
                int32_t norminfoindex; // normal bone info
                int32_t normindex; // normal vec3

                size_t numgroups; // deformation groups
                int32_t groupindex;
            };

            struct HlMdlBodyParts { // This is the "mstudiobodyparts_t" structure
                std::string name; // Originally char[64]
                size_t nummodels;
                int32_t base;
                int32_t modelindex; // index into models array
            };

            HlMdlHeader m_modelHeader;
            HlMdlHeader m_textureHeader;
            HlMdlSequenceHeaderList m_sequenceHeaders;

            const std::string m_name;
            const char* m_begin;
            const char* m_end;
            const std::string m_extension;
            const FileSystem& m_fs;
        public:
            HlMdlParser(const std::string& name, const char* begin, const char* end, const std::string& extension, const FileSystem& fs);
        private:
            std::unique_ptr<Assets::EntityModel> doInitializeModel(Logger& logger) override;
            void doLoadFrame(size_t frameIndex, Assets::EntityModel& model, Logger& logger) override;

            HlMdlBodyParts getBodyPartById(Reader& reader, unsigned int id);
            size_t getTotalFramesCount(Reader& reader);

            void loadExternalTexturesModelFile(const std::string& baseFileName);
            void loadSequenceModelFiles(const std::string& baseFileName);

            HlMdlHeader parseHeader(Reader& reader, const bool sequenceFile);
            HlMdlBone parseBone(Reader& reader);
            HlMdlSequenceDescription parseSequenceDescription(Reader& reader);
            HlMdlModel parseModel(Reader& reader);
            HlMdlBodyParts parseBodyParts(Reader& reader);
        };
    }
}

#endif /* defined(TrenchBroom_HlMdlParser) */
