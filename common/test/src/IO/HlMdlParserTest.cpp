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

#include <catch2/catch.hpp>

#include "GTestCompat.h"

#include "Logger.h"
#include "IO/DiskFileSystem.h"
#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/HlMdlParser.h"
#include "Assets/EntityModel.h"

#include <kdl/string_format.h>

namespace TrenchBroom {
    namespace IO {
        TEST_CASE("HlMdlParserTest.loadValidExternalizedSequencesMdl", "[HlMdlParserTest]") {
            NullLogger logger;

            DiskFileSystem fs(IO::Disk::getCurrentWorkingDir());
            const auto basePath = IO::Path("fixture/test/IO/HlMdl/cube_external_sequences.mdl");
            const auto mdlPath = IO::Disk::getCurrentWorkingDir() + basePath;
            const auto mdlFile = Disk::openFile(mdlPath);
            ASSERT_NE(nullptr, mdlFile);

            const auto extension = kdl::str_to_lower(mdlPath.extension());

            auto reader = mdlFile->reader().buffer();
            auto parser = HlMdlParser("cube_external_sequences", std::begin(reader), std::end(reader), fs, extension, basePath.asString());
            auto model = parser.initializeModel(logger);

            EXPECT_NE(nullptr, model);
            // TODO: Complete this test with more checks
        }

        TEST_CASE("HlMdlParserTest.loadValidExternalizedTexturesMdl", "[HlMdlParserTest]") {
            NullLogger logger;

            DiskFileSystem fs(IO::Disk::getCurrentWorkingDir());
            const auto basePath = IO::Path("fixture/test/IO/HlMdl/cube_external_textures.mdl");
            const auto mdlPath = IO::Disk::getCurrentWorkingDir() + basePath;
            const auto mdlFile = Disk::openFile(mdlPath);
            ASSERT_NE(nullptr, mdlFile);

            const auto extension = kdl::str_to_lower(mdlPath.extension());

            auto reader = mdlFile->reader().buffer();
            auto parser = HlMdlParser("cube_external_textures", std::begin(reader), std::end(reader), fs, extension, basePath.asString());
            auto model = parser.initializeModel(logger);

            EXPECT_NE(nullptr, model);
            // TODO: Complete this test with more checks
        }

        TEST_CASE("HlMdlParserTest.loadValidNormalMdl", "[HlMdlParserTest]") {
            NullLogger logger;

            DiskFileSystem fs(IO::Disk::getCurrentWorkingDir());
            const auto basePath = IO::Path("fixture/test/IO/HlMdl/cube_normal.mdl");
            const auto mdlPath = IO::Disk::getCurrentWorkingDir() + basePath;
            const auto mdlFile = Disk::openFile(mdlPath);
            ASSERT_NE(nullptr, mdlFile);

            const auto extension = kdl::str_to_lower(mdlPath.extension());

            auto reader = mdlFile->reader().buffer();
            auto parser = HlMdlParser("cube_normal", std::begin(reader), std::end(reader), fs, extension, basePath.asString());
            auto model = parser.initializeModel(logger);

            EXPECT_NE(nullptr, model);
            // TODO: Complete this test with more checks
        }

        TEST_CASE("HlMdlParserTest.loadInvalidMdl", "[HlMdlParserTest]") {
            NullLogger logger;

            DiskFileSystem fs(IO::Disk::getCurrentWorkingDir());
            const auto basePath = IO::Path("fixture/test/IO/HlMdl/cube_invalid.mdl");
            const auto mdlPath = IO::Disk::getCurrentWorkingDir() + basePath;
            const auto mdlFile = Disk::openFile(mdlPath);
            ASSERT_NE(nullptr, mdlFile);

            const auto extension = kdl::str_to_lower(mdlPath.extension());

            auto reader = mdlFile->reader().buffer();
            auto parser = HlMdlParser("cube_invalid", std::begin(reader), std::end(reader), fs, extension, basePath.asString());
            EXPECT_THROW(parser.initializeModel(logger), AssetException);
        }
    }
}
