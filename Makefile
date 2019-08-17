#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := shroomlight

include $(IDF_PATH)/make/project.mk

incrementversion:
	@echo Increment version
	$$(./incrementversion.sh)

# Hook on an early target and increment version
check_python_dependencies: incrementversion

