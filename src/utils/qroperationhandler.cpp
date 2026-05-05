/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qroperationhandler.h"
#include "qroperationconfig.h"
#include "qrcommandprocessor.h"
#include <layers/baselayer.h>
#include <layers/texturelayer.h>
#include <presentationsettings.h>
#include <sgct/sgct.h>

QROperationHandler::QROperationHandler() {
    m_config = new QROperationConfig();
}

QROperationHandler::~QROperationHandler() {
    m_subLayers.clear();
    delete m_config;
}

bool QROperationHandler::loadConfig(const std::string& filePath) {
    if (m_config) {
        return m_config->loadFromFile(filePath);
    }
    return false;
}

const QROperationConfig* QROperationHandler::config() const {
    return m_config;
}

void QROperationHandler::handleCommand(const QRCommand& command,
                                       BaseLayer* parentLayer,
                                       unsigned int liveTex, int liveW, int liveH) {
    sgct::Log::Info(std::format("QROperationHandler: target='{}', actions={}",
        command.target, command.actions.size()));

    GLenum parentFormat = static_cast<GLenum>(parentLayer->textureInternalFormat());

    for (const auto& action : command.actions) {
        sgct::Log::Info(std::format("  action: {}", action));

        if (action == "SetActive") {
            if (command.target == "All") {
                sgct::Log::Info("QROperationHandler: SetActive not valid for target 'All'");
                continue;
            }

            int fadeDurationMs = PresentationSettings::fadeDurationToNextSlide();

            // Freeze all currently unfrozen sublayers (capture their current frame)
            for (auto& sub : m_subLayers) {
                auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
                if (texSub && !texSub->isFrozen()) {
                    texSub->copyFromTexture(liveTex, liveW, liveH, parentFormat);
                    texSub->setFrozen(true);
                }
            }

            // Freeze the current active plane if different from the target
            if (!m_activePlaneName.empty() && m_activePlaneName != command.target) {
                freezeToSubLayer(m_activePlaneName, parentLayer, liveTex, liveW, liveH);
            }

            // Check if the target sublayer already exists
            bool targetExists = false;
            for (auto& sub : m_subLayers) {
                if (sub->title() == command.target) {
                    targetExists = true;
                    auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
                    if (texSub) {
                        texSub->setFrozen(false);
                        // Fade in from current alpha to parent alpha
                        texSub->startFadeTo(parentLayer->alpha(), fadeDurationMs);
                    }
                    break;
                }
            }

            if (!targetExists) {
                auto newSub = std::make_shared<TextureLayer>(command.target);
                newSub->initialize();

                newSub->setGridMode(parentLayer->gridMode());
                newSub->setStereoMode(parentLayer->stereoMode());
                newSub->setRotate(const_cast<glm::vec3&>(parentLayer->rotate()));
                newSub->setTranslate(const_cast<glm::vec3&>(parentLayer->translate()));

                applyPlaneProperties(command.target, parentLayer, newSub);

                if (parentLayer->roiEnabled()) {
                    newSub->setRoiEnabled(true);
                    newSub->setRoi(const_cast<glm::vec4&>(parentLayer->roi()));
                }

                // Start at alpha 0 and fade in
                newSub->setAlpha(0.f);
                newSub->startFadeTo(parentLayer->alpha(), fadeDurationMs);

                m_subLayers.push_back(newSub);
            }

            m_activePlaneName = command.target;

            sgct::Log::Info(std::format("QROperationHandler: SetActive '{}' (sublayers: {}, fade: {}ms)",
                command.target, m_subLayers.size(), fadeDurationMs));
        }
        else if (action == "Clear") {
            int fadeDurationMs = PresentationSettings::fadeDurationToNextSlide();

            if (command.target == "All" || command.target == "AllCaptures") {
                for (auto& sub : m_subLayers) {
                    auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
                    if (texSub) {
                        texSub->startFadeTo(0.f, fadeDurationMs);
                    }
                }
                m_activePlaneName.clear();
                sgct::Log::Info(std::format("QROperationHandler: Clear '{}' (fade: {}ms)", command.target, fadeDurationMs));
            }
            else {
                for (auto& sub : m_subLayers) {
                    if (sub->title() == command.target) {
                        auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
                        if (texSub) {
                            texSub->startFadeTo(0.f, fadeDurationMs);
                        }
                        break;
                    }
                }
                if (m_activePlaneName == command.target) {
                    m_activePlaneName.clear();
                }
                sgct::Log::Info(std::format("QROperationHandler: Clear '{}' (fade: {}ms)", command.target, fadeDurationMs));
            }
        }
        else if (action == "Freeze") {
            freezeToSubLayer(command.target, parentLayer, liveTex, liveW, liveH);
        }
        else if (action == "Unfreeze") {
            unfreezeSubLayer(command.target);
        }
        else if (action == "UnfreezeAll") {
            unfreezeAll();
            m_activePlaneName.clear();
        }
    }
}

void QROperationHandler::updateActiveSubLayer(BaseLayer* parentLayer,
                                              unsigned int liveTex, int liveW, int liveH) {
    // Tick fades on all sublayers
    for (auto& sub : m_subLayers) {
        auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
        if (texSub) {
            texSub->tickFade();
        }
    }

    if (m_activePlaneName.empty() || liveTex == 0 || liveW <= 0 || liveH <= 0)
        return;

    GLenum parentFormat = static_cast<GLenum>(parentLayer->textureInternalFormat());

    for (auto& sub : m_subLayers) {
        if (sub->title() == m_activePlaneName) {
            auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
            if (texSub && !texSub->isFrozen()) {
                // Copy the parent's live texture into the active sublayer every frame
                texSub->copyFromTexture(liveTex, liveW, liveH, parentFormat);

                // Only set alpha directly if no fade is in progress
                if (!texSub->isFading()) {
                    texSub->setAlpha(parentLayer->alpha());
                }

                if (parentLayer->gridMode() == BaseLayer::GridMode::Plane) {
                    texSub->updatePlane();
                }
            }
            break;
        }
    }
}

bool QROperationHandler::isActive() const {
    return !m_subLayers.empty();
}

bool QROperationHandler::hasSubLayers() const {
    return !m_subLayers.empty();
}

std::vector<std::shared_ptr<BaseLayer>>& QROperationHandler::subLayers() const {
    return m_subLayers;
}

std::string QROperationHandler::activePlaneName() const {
    return m_activePlaneName;
}

void QROperationHandler::clearAll() {
    m_subLayers.clear();
    m_activePlaneName.clear();
    sgct::Log::Info("QROperationHandler::clearAll: All sublayers removed");
}

bool QROperationHandler::freezeToSubLayer(const std::string& name,
                                          BaseLayer* parentLayer,
                                          unsigned int liveTex, int liveW, int liveH) {
    if (liveTex == 0 || liveW <= 0 || liveH <= 0) {
        sgct::Log::Error(std::format("QROperationHandler::freezeToSubLayer: No valid texture for '{}'", name));
        return false;
    }

    GLenum parentFormat = static_cast<GLenum>(parentLayer->textureInternalFormat());

    std::shared_ptr<TextureLayer> existing = nullptr;
    for (auto& sub : m_subLayers) {
        if (sub->title() == name) {
            existing = std::dynamic_pointer_cast<TextureLayer>(sub);
            break;
        }
    }

    if (existing) {
        existing->copyFromTexture(liveTex, liveW, liveH, parentFormat);
        existing->setFrozen(true);
        existing->setAlpha(parentLayer->alpha());
        sgct::Log::Info(std::format("QROperationHandler::freezeToSubLayer: Re-frozen '{}'", name));

        if (parentLayer->gridMode() == BaseLayer::GridMode::Plane) {
            existing->updatePlane();
        }
    }
    else {
        auto frozen = std::make_shared<TextureLayer>(name);
        frozen->initialize();

        frozen->setGridMode(parentLayer->gridMode());
        frozen->setStereoMode(parentLayer->stereoMode());
        frozen->setRotate(const_cast<glm::vec3&>(parentLayer->rotate()));
        frozen->setTranslate(const_cast<glm::vec3&>(parentLayer->translate()));
        frozen->setAlpha(parentLayer->alpha());

        applyPlaneProperties(name, parentLayer, frozen);

        if (parentLayer->roiEnabled()) {
            frozen->setRoiEnabled(true);
            frozen->setRoi(const_cast<glm::vec4&>(parentLayer->roi()));
        }

        if (!frozen->copyFromTexture(liveTex, liveW, liveH, parentFormat)) {
            sgct::Log::Error(std::format("QROperationHandler::freezeToSubLayer: Failed capture for '{}'", name));
            return false;
        }

        frozen->setFrozen(true);

        if (parentLayer->gridMode() == BaseLayer::GridMode::Plane) {
            frozen->updatePlane();
        }

        m_subLayers.push_back(frozen);
        sgct::Log::Info(std::format("QROperationHandler::freezeToSubLayer: Created '{}' (total: {})", name, m_subLayers.size()));
    }

    return true;
}

void QROperationHandler::unfreezeSubLayer(const std::string& name) {
    for (auto& sub : m_subLayers) {
        if (sub->title() == name) {
            auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
            if (texSub) {
                texSub->setFrozen(false);
            }
            sgct::Log::Info(std::format("QROperationHandler::unfreezeSubLayer: Unfrozen '{}'", name));
            return;
        }
    }
    sgct::Log::Info(std::format("QROperationHandler::unfreezeSubLayer: '{}' not found", name));
}

void QROperationHandler::unfreezeAll() {
    for (auto& sub : m_subLayers) {
        auto texSub = std::dynamic_pointer_cast<TextureLayer>(sub);
        if (texSub) {
            texSub->setFrozen(false);
        }
    }
    sgct::Log::Info(std::format("QROperationHandler::unfreezeAll: Unfrozen {} sublayers", m_subLayers.size()));
}

void QROperationHandler::applyPlaneProperties(const std::string& planeName,
                                              BaseLayer* parentLayer,
                                              std::shared_ptr<TextureLayer>& sub) {
    if (parentLayer->gridMode() != BaseLayer::GridMode::Plane)
        return;

    // Start from the parent layer's values as the baseline
    double azimuth   = parentLayer->planeAzimuth();
    double elevation = parentLayer->planeElevation();
    double roll      = parentLayer->planeRoll();
    double distance  = parentLayer->planeDistance();
    double height    = parentLayer->planeHeight();

    // Override only fields that are explicitly specified in the config
    const QRPlaneDefinition* planeDef = nullptr;
    if (m_config) {
        planeDef = m_config->findPlane(planeName);
    }

    if (planeDef) {
        if (planeDef->has(QRPlaneDefinition::Azimuth))
            azimuth = planeDef->azimuth;
        if (planeDef->has(QRPlaneDefinition::Elevation))
            elevation = planeDef->elevation;
        if (planeDef->has(QRPlaneDefinition::Roll))
            roll = planeDef->roll;
        if (planeDef->has(QRPlaneDefinition::Distance))
            distance = planeDef->distance;
        if (planeDef->has(QRPlaneDefinition::Height))
            height = planeDef->height;
    }

    sub->setPlaneAzimuth(azimuth);
    sub->setPlaneElevation(elevation);
    sub->setPlaneRoll(roll);
    sub->setPlaneDistance(distance);

    // If height was overridden from config, derive width from aspect ratio;
    // otherwise copy the parent's full size and offsets.
    if (planeDef && planeDef->has(QRPlaneDefinition::Height)) {
        sub->setPlaneSize(glm::vec2(static_cast<float>(height) * 16.f / 9.f,
                                     static_cast<float>(height)), parentLayer->planeAspectRatio());
    }
    else {
        sub->setPlaneHorizontal(parentLayer->planeHorizontal());
        sub->setPlaneVertical(parentLayer->planeVertical());
        sub->setPlaneSize(glm::vec2(static_cast<float>(parentLayer->planeWidth()),
                                     static_cast<float>(parentLayer->planeHeight())), parentLayer->planeAspectRatio());
    }

    sub->updatePlane();
}
