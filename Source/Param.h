#pragma once
#include <JuceHeader.h>

namespace param {
	enum class ID { Speed, Phase };

	static juce::String getName(ID i) {
		switch (i) {
		case ID::Speed: return "Speed";
		case ID::Phase: return "Phase";
		default: return "";
		}
	}
	static juce::String getName(int i) { getName(static_cast<ID>(i)); }
	static juce::String getID(const ID i) { return getName(i).toLowerCase().removeCharacters(" "); }
	static juce::String getID(const int i) { return getName(i).toLowerCase().removeCharacters(" "); }

	static std::unique_ptr<juce::AudioParameterBool> createPBool(ID i, bool defaultValue, std::function<juce::String(bool value, int maxLen)> func) {
		return std::make_unique<juce::AudioParameterBool>(
			getID(i), getName(i), defaultValue, getName(i), func
			);
	}
	static std::unique_ptr<juce::AudioParameterChoice> createPChoice(ID i, const juce::StringArray& choices, int defaultValue) {
		return std::make_unique<juce::AudioParameterChoice>(
			getID(i), getName(i), choices, defaultValue, getName(i)
			);
	}
	static std::unique_ptr<juce::AudioParameterFloat> createParameter(ID i, const juce::NormalisableRange<float>& range, float defaultValue,
		std::function<juce::String(float value, int maxLen)> stringFromValue = nullptr) {
		return std::make_unique<juce::AudioParameterFloat>(
			getID(i), getName(i), range, defaultValue, getName(i), juce::AudioProcessorParameter::Category::genericParameter,
			stringFromValue
			);
	}

	static juce::AudioProcessorValueTreeState::ParameterLayout createParameters() {
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

		auto speedStr = [](float value, int) {
			auto convValue = std::pow(2.f, value);
			if (value < 0) {
				convValue = 1.f / convValue;
				return juce::String("1/") + juce::String(convValue) + "x";
			}	
			return juce::String(static_cast<int>(convValue)) + "x";
		};

		auto phaseStr = [](float value, int) {
			return juce::String(std::floor(value * 360.f)) + "°";
		};

		parameters.push_back(createParameter(ID::Speed, juce::NormalisableRange<float>(-2, 2, 1), 0, speedStr));
		parameters.push_back(createParameter(ID::Phase, juce::NormalisableRange<float>(0, 1, 1.f / 360.f), 0, phaseStr));
		
		return { parameters.begin(), parameters.end() };
	}
};