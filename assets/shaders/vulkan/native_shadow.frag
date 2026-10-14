#version 450
layout(location=0) flat in int material;
void main(){if(material==15)discard;}
