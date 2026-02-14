TARGET := iphone:clang:latest:16.3
INSTALL_TARGET_PROCESSES = SpringBoard
ARCHS = arm64

include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = Dumper

Dumper_FILES = Dumper/main.mm \
	Dumper/Utils/Dumpspace/DSGen.cpp \
	Dumper/Generator/Private/Managers/EnumManager.cpp \
	Dumper/Generator/Private/Managers/CollisionManager.cpp \
	Dumper/Generator/Private/Managers/PackageManager.cpp \
	Dumper/Generator/Private/Managers/MemberManager.cpp \
	Dumper/Generator/Private/Managers/DependencyManager.cpp \
	Dumper/Generator/Private/Managers/StructManager.cpp \
	Dumper/Generator/Private/Wrappers/EnumWrapper.cpp \
	Dumper/Generator/Private/Wrappers/MemberWrappers.cpp \
	Dumper/Generator/Private/Wrappers/StructWrapper.cpp \
	Dumper/Generator/Private/Generators/MappingGenerator.cpp \
	Dumper/Generator/Private/Generators/DumpspaceGenerator.cpp \
	Dumper/Generator/Private/Generators/Generator.cpp \
	Dumper/Generator/Private/Generators/IDAMappingGenerator.cpp \
	Dumper/Generator/Private/Generators/CppGenerator.cpp \
	Dumper/Generator/Private/HashStringTable.cpp \
	Dumper/Engine/Private/OffsetFinder/OffsetFinder.cpp \
	Dumper/Engine/Private/OffsetFinder/Offsets.cpp \
	Dumper/Engine/Private/Unreal/UnrealTypes.cpp \
	Dumper/Engine/Private/Unreal/NameArray.cpp \
	Dumper/Engine/Private/Unreal/UnrealObjects.cpp \
	Dumper/Engine/Private/Unreal/ObjectArray.cpp \
	Dumper/ImGui/imgui_tables.cpp \
	Dumper/ImGui/imgui.cpp \
	Dumper/ImGui/imgui_draw.cpp \
	Dumper/ImGui/imgui_demo.cpp \
	Dumper/ImGui/imgui_impl_metal.mm \
	Dumper/ImGui/imgui_widgets.cpp \
	Dumper/MenuLoad/ImGuiDrawView.mm \
	Dumper/MenuLoad/MenuLoad.mm \
	Dumper/Menu/Logger.mm \
	Dumper/Menu/UserMenu.mm \
	Dumper/Menu/Console.mm

Dumper_CFLAGS = -fobjc-arc -std=c++17
Dumper_CCFLAGS = -std=c++17
Dumper_FRAMEWORKS = Foundation UIKit Metal MetalKit
Dumper_LDFLAGS = -all_load
Dumper_INSTALL_PATH = /Library/MobileSubstrate/DynamicLibraries

include $(THEOS_MAKE_PATH)/library.mk
