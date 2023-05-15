#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_tools.hpp"
#include "gltf_path.hpp"

template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, float, AccessorType::Vec3> {};

static const std::byte* getBufferData(const fastgltf::Buffer& buffer) {
	const std::byte* result = nullptr;

	std::visit(fastgltf::visitor {
		[](auto&) {},
		[&](const fastgltf::sources::Vector& vec) {
			result = reinterpret_cast<const std::byte*>(vec.bytes.data());
		},
		[&](const fastgltf::sources::ByteView& bv) {
			result = bv.bytes.data();
		},
	}, buffer.data);
	
	return result;
}

TEST_CASE("Test accessor", "[gltf-tools]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto model = parser.loadGLTF(&jsonData, lightsLamp, fastgltf::Options::LoadExternalBuffers);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Buffers | fastgltf::Category::BufferViews
			| fastgltf::Category::Accessors) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->accessors.size() == 15);
    auto& accessors = asset->accessors;

    SECTION("getAccessorElement<std::uint16_t>") {
        auto& firstAccessor = accessors[0];
		REQUIRE(firstAccessor.type == fastgltf::AccessorType::Scalar);
		REQUIRE(firstAccessor.componentType == fastgltf::ComponentType::UnsignedShort);

		REQUIRE(firstAccessor.bufferViewIndex.has_value());
		auto& view = asset->bufferViews[*firstAccessor.bufferViewIndex];

		auto* bufferData = getBufferData(asset->buffers[view.bufferIndex]);
		REQUIRE(bufferData != nullptr);

		auto* checkData = reinterpret_cast<const std::uint16_t*>(bufferData + view.byteOffset
				+ firstAccessor.byteOffset);

		REQUIRE(*checkData == fastgltf::getAccessorElement<std::uint16_t>(*asset, firstAccessor, 0));
    }

	{
        auto& secondAccessor = accessors[1];
		REQUIRE(secondAccessor.type == fastgltf::AccessorType::Vec3);
		REQUIRE(secondAccessor.componentType == fastgltf::ComponentType::Float);

		REQUIRE(secondAccessor.bufferViewIndex.has_value());
		auto& view = asset->bufferViews[*secondAccessor.bufferViewIndex];

		auto* bufferData = getBufferData(asset->buffers[view.bufferIndex]);
		REQUIRE(bufferData != nullptr);

		auto* checkData = reinterpret_cast<const glm::vec3*>(bufferData + view.byteOffset
				+ secondAccessor.byteOffset);

		SECTION("getAccessorElement<glm::vec3>") {
			REQUIRE(*checkData == fastgltf::getAccessorElement<glm::vec3>(*asset, secondAccessor, 0));
		}

		SECTION("iterateAccessor") {
			auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
			std::size_t i = 0;

			fastgltf::iterateAccessor<glm::vec3>(*asset, secondAccessor, [&](auto&& v3) {
				dstCopy[i++] = std::forward<glm::vec3>(v3);
			});

			REQUIRE(std::memcmp(dstCopy.get(), checkData, secondAccessor.count * sizeof(glm::vec3)) == 0);
		}

		SECTION("copyFromAccessor") {
			auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
			fastgltf::copyFromAccessor<glm::vec3>(*asset, secondAccessor, dstCopy.get());
			REQUIRE(std::memcmp(dstCopy.get(), checkData, secondAccessor.count * sizeof(glm::vec3)) == 0);
		}
	}
}

TEST_CASE("Test sparse accessor", "[gltf-tools]") {
    auto simpleSparseAccessor = sampleModels / "2.0" / "SimpleSparseAccessor" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(simpleSparseAccessor / "SimpleSparseAccessor.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), simpleSparseAccessor, fastgltf::Options::LoadExternalBuffers);
    REQUIRE(model != nullptr);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Buffers | fastgltf::Category::BufferViews
			| fastgltf::Category::Accessors) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->accessors.size() == 2);
    REQUIRE(!asset->accessors[0].sparse.has_value());
    REQUIRE(asset->accessors[1].sparse.has_value());
    auto& sparse = asset->accessors[1].sparse.value();
    REQUIRE(sparse.count == 3);
    REQUIRE(sparse.bufferViewIndices == 2);
    REQUIRE(sparse.byteOffsetIndices == 0);
    REQUIRE(sparse.bufferViewValues == 3);
    REQUIRE(sparse.byteOffsetValues == 0);
    REQUIRE(sparse.indexComponentType == fastgltf::ComponentType::UnsignedShort);


	auto& secondAccessor = asset->accessors[1];
	auto& viewIndices = asset->bufferViews[secondAccessor.sparse->bufferViewIndices];
	auto& viewValues = asset->bufferViews[secondAccessor.sparse->bufferViewValues];

	auto& viewData = asset->bufferViews[*secondAccessor.bufferViewIndex];
	auto* bufferData = getBufferData(asset->buffers[viewData.bufferIndex]) + viewData.byteOffset
			+ secondAccessor.byteOffset;
	auto dataStride = viewData.byteStride ? *viewData.byteStride
			: fastgltf::getElementByteSize(secondAccessor.type, secondAccessor.componentType);

	auto* dataIndices = reinterpret_cast<const std::uint16_t*>(getBufferData(asset->buffers[viewIndices.bufferIndex])
			+ viewIndices.byteOffset + secondAccessor.sparse->byteOffsetIndices);
	auto* dataValues = reinterpret_cast<const glm::vec3*>(getBufferData(asset->buffers[viewValues.bufferIndex])
			+ viewValues.byteOffset + secondAccessor.sparse->byteOffsetValues);

	auto checkValues = std::make_unique<glm::vec3[]>(secondAccessor.count);

	for (std::size_t i = 0, sparseIndex = 0; i < secondAccessor.count; ++i) {
		if (sparseIndex < secondAccessor.sparse->count && dataIndices[sparseIndex] == i) {
			checkValues[i] = dataValues[sparseIndex];
			++sparseIndex;
		} else {
			checkValues[i] = *reinterpret_cast<const glm::vec3*>(bufferData + dataStride * i);
		}
	}

	SECTION("getAccessorElement") {
		for (std::size_t i = 0; i < secondAccessor.count; ++i) {
			REQUIRE(checkValues[i] == fastgltf::getAccessorElement<glm::vec3>(*asset, secondAccessor, i));
		}
	}

	SECTION("iterateAccessor") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
		std::size_t i = 0;

		fastgltf::iterateAccessor<glm::vec3>(*asset, secondAccessor, [&](auto&& v3) {
			dstCopy[i++] = std::forward<glm::vec3>(v3);
		});

		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}

	SECTION("copyFromAccessor") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
		fastgltf::copyFromAccessor<glm::vec3>(*asset, secondAccessor, dstCopy.get());
		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}
}
