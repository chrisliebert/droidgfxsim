#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "application/application.h"
#include "common/log.h"
#include "stb_image.h"

#define XML_FILENAME "data.xml"

Image* Application::loadImage(const char* filename) {
	Image* image = new Image();
	assert(image);
#ifndef DESKTOP_APP
	AAsset* file = AAssetManager_open(asset_manager, filename, AASSET_MODE_BUFFER);
	size_t file_length = AAsset_getLength(file);
	assert(file_length > 0);
	unsigned char* image_file_bytes = new unsigned char[file_length];
	assert(image_file_bytes);
	AAsset_read(file, image_file_bytes, file_length);
	AAsset_close(file);
	image->data = stbi_load_from_memory(image_file_bytes, file_length * sizeof(unsigned char), &image->w, &image->h, &image->comp, STBI_default);
#else
	image->data = stbi_load(filename, &image->w, &image->h, &image->comp,
			STBI_default);
#endif
	if (image->data == 0) {
		LOGE("Unable to load image: %s", filename);
		return 0;
	}
	return image;
}

Node* Application::loadResources() {
	return loadXML(XML_FILENAME);
}

#ifndef DESKTOP_APP
Application::Application(AAssetManager* asset_manager) {
	config_file_contents = 0;
	this->asset_manager = asset_manager;
	assert(this->asset_manager);
	simulation = new Simulation();
	assert(simulation);
	scenegraph_root = loadResources();
	assert(scenegraph_root);
	init();
	if(config_file_contents) {
		delete [] config_file_contents;
	}
}

#else

Application::Application() {
	config_file_contents = 0;
	simulation = new Simulation();
	assert(simulation);
	scenegraph_root = loadResources();
	assert(scenegraph_root);
	init();
	if (config_file_contents) {
		delete[] config_file_contents;
	}
}
#endif

Application::~Application() {
	if (simulation)
		delete simulation;
	if (scenegraph_root) {
		scenegraph::destroy(scenegraph_root);
		scenegraph_root = 0;
	}
	for (std::map<std::string, Image*>::iterator it = images.begin();
			it != images.end(); ++it) {
		if (it->second) {
			if (it->second->data) {
				delete it->second->data;
				it->second->data = 0;
			}
			delete it->second;
			it->second = 0;
		}
	}
	images.clear();
}

Node* Application::parseXML(rapidxml::xml_document<>& doc) {
	Node* scene_node = new Node();
	assert(scene_node);
	scene_node->name = std::string(doc.name()) + std::string(" node");
	parseXMLNode(doc.first_node(), scene_node);
	simulation->parseXMLNode(doc.first_node(), scene_node);
	return scene_node;
}

void Application::parseXMLNode(rapidxml::xml_node<>* my_xml_node,
		scenegraph::Node* scene_node) {
	if (!my_xml_node)
		return;
	char* c_name = my_xml_node->name();
	if (0 == strlen(c_name))
		return;
	std::string name(c_name);
	if (0 == std::string("WavefrontFile").compare(name)) {
		for (rapidxml::xml_attribute<> *attr = my_xml_node->first_attribute();
				attr; attr = attr->next_attribute()) {
			if (0 == std::string("filename").compare(attr->name())) {
				WavefrontSceneGraphFactory factory;
#ifndef DESKTOP_APP
				bool status = factory.addWavefront(attr->value(), glm::mat4(1.f), asset_manager);
#else
				bool status = factory.addWavefront(attr->value(),
						glm::mat4(1.f));
#endif
				assert(status);
				for (std::set<std::string>::iterator it =
						factory.textures.begin(); it != factory.textures.end();
						++it) {
					std::string s = *it;
					Image* t = loadImage(s.c_str());
					images[s] = t;
				}
				Node* wf = factory.build();
				assert(wf);
				scene_node->children.push_back(wf);
			}
		}
	} else if (0 == std::string("InstanceNode").compare(name)) {
		LOGI("Instance Node %s", my_xml_node->value());
	}

	// Recursively parse children and siblings
	parseXMLNode(my_xml_node->first_node(), scene_node);
	parseXMLNode(my_xml_node->next_sibling(), scene_node);
}

Node* Application::loadXML(const char* xml_filename) {
	rapidxml::xml_document<> doc;

#ifndef DESKTOP_APP
	AAsset* file = AAssetManager_open(asset_manager, xml_filename, AASSET_MODE_BUFFER);
	size_t file_length = AAsset_getLength(file);
	config_file_contents = new char[file_length + 1];
	assert(config_file_contents);
	AAsset_read(file, config_file_contents, file_length);
	AAsset_close(file);
	doc.parse<0>(config_file_contents);
#else
	{
		std::ifstream filestream(xml_filename);
		std::vector<char> buffer((std::istreambuf_iterator<char>(filestream)),
				std::istreambuf_iterator<char>());
		filestream.close();
		buffer.push_back('\0');
		config_file_contents = new char[buffer.size()];
		assert(config_file_contents);
		memcpy(config_file_contents, buffer.data(), buffer.size());
		doc.parse<0>(config_file_contents);
	}
#endif
	return parseXML(doc);
}

void Application::init() {
	// Init camera
	camera = new Camera();
	assert(camera);
	camera->position.y = 4.0;
	camera->moveBackward(28.0);
	camera->update();
}

void Application::resize(int width, int height) {
	assert(camera);
	camera->projectionMatrix = glm::perspective(45.0f,
			(float) ((double) width / (double) height), 0.1f, 10000.0f);
	camera->update();
}

void Application::step() {
	grey += 0.01f;
	if (grey > 1.0f) {
		grey = 0.0f;
	}
	if (!simulation)
		return;

	simulation->step();

	// Update scenegraph with physics simulation data
	for (int j = simulation->dynamics_world->getNumCollisionObjects() - 1;
			j >= 0; j--) {
		btCollisionObject* obj =
				simulation->dynamics_world->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState()) {
			body->getMotionState()->getWorldTransform(trans);
		} else {
			trans = obj->getWorldTransform();
		}

		std::map<int, PhysicsNode>::iterator itr =
				simulation->physics_nodes.find(j);
		if (itr != simulation->physics_nodes.end() && itr->second.mass > 0.f) {
			btVector3 origin_bt = trans.getOrigin();
			glm::vec3 pos((float) origin_bt.x(), (float) origin_bt.y(),
					(float) origin_bt.z());
			glm::mat4 pos_mat = glm::translate(glm::mat4(1.0f), pos);

			btQuaternion rotation_bt = trans.getRotation();
			glm::quat quat(rotation_bt.w(), rotation_bt.x(), rotation_bt.y(),
					rotation_bt.z());
			glm::mat4 rotation = glm::toMat4(quat);
			itr->second.transform_node->matrix = pos_mat * rotation;
		}
	}
}
