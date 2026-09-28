#include "mesh.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

mesh::mesh(const std::string& file_name)
	:name(file_name){
	std::ifstream in(name);
	if(!in){
		throw std::invalid_argument("could not open file " + name);
	}

	std::string line;
	while(std::getline(in,line)){
		if(line.compare(0,2,"v ") == 0){
			std::string tag;
			std::istringstream iss(line);
			float a = 0.0f;
			float b = 0.0f;
			float c = 0.0f;
			iss>>tag>>a>>b>>c;
			vec3 temp(a,b,c);
			vertices.push_back(temp);
		}else if(line.compare(0,2,"f ") == 0){
			// "f 1 2 3" or "f 1/4/7 2/5/8 3/6/9": the vertex index is the
			// number before the first '/'. Only triangles are supported.
			std::istringstream iss(line.substr(2));
			std::string token;
			int index[3];
			for(int k = 0;k<3;k++){
				if(!(iss>>token)){
					throw std::runtime_error("face needs 3 vertices in " + name + ": " + line);
				}
				index[k] = std::stoi(token.substr(0, token.find('/')));
			}
			triangle temp(index[0],index[1],index[2]);
			faces.push_back(temp);
		}
	}
	in.close();

	// .obj indices start at 1
	for(const triangle& t : faces){
		for(int k = 0;k<3;k++){
			if(t[k] < 1 || t[k] > get_vertices_count()){
				throw std::out_of_range("face uses a vertex that doesn't exist in " + name);
			}
		}
	}
}
int mesh::get_vertices_count()const{
	return vertices.size();
}
int mesh::get_faces_count()const{
	return faces.size();
}
vec3 mesh::vertex(int i)const{
	return vertices[i];
}
triangle mesh::face(int i)const{
	return faces[i];
}
