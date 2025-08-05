#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(p2p_logger
    logger.cpp
    configurator.cpp
    )
target_link_libraries(p2p_logger
    soralog::soralog
    soralog::fallback_configurator
    soralog::configurator_yaml
    )
