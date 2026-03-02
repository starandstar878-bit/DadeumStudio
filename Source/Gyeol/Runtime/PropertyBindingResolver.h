#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <cstdlib>
#include <map>

namespace Gyeol::Runtime
{
    struct PropertyBindingEvaluation
    {
        bool success = false;
        double value = 0.0;
        juce::String error;
    };

    class PropertyBindingResolver
    {
    public:
        static PropertyBindingEvaluation evaluateExpression(const juce::String& expression,
                                                            const std::map<juce::String, juce::var>& runtimeParams)
        {
            Parser parser(expression, runtimeParams);
            return parser.parse();
        }

    private:
        class Parser
        {
        public:
            Parser(const juce::String& expressionIn,
                   const std::map<juce::String, juce::var>& runtimeParamsIn)
                : expression(expressionIn),
                  runtimeParams(runtimeParamsIn)
            {
            }

            PropertyBindingEvaluation parse()
            {
                PropertyBindingEvaluation result;
                skipWhitespace();

                if (expression.trim().isEmpty())
                {
                    result.error = "expression is empty";
                    return result;
                }

                double parsedValue = 0.0;
                if (!parseExpression(parsedValue))
                {
                    result.error = errorText.isNotEmpty() ? errorText : "failed to parse expression";
                    return result;
                }

                skipWhitespace();
                if (!isAtEnd())
                {
                    result.error = "unexpected token near '" + juce::String::charToString(currentChar()) + "'";
                    return result;
                }

                if (!std::isfinite(parsedValue))
                {
                    result.error = "expression result is not finite";
                    return result;
                }

                result.success = true;
                result.value = parsedValue;
                return result;
            }

        private:
            const juce::String& expression;
            const std::map<juce::String, juce::var>& runtimeParams;
            int position = 0;
            juce::String errorText;

            bool parseExpression(double& outValue)
            {
                if (!parseTerm(outValue))
                    return false;

                while (true)
                {
                    skipWhitespace();
                    if (match('+'))
                    {
                        double rhs = 0.0;
                        if (!parseTerm(rhs))
                            return false;
                        outValue += rhs;
                        continue;
                    }

                    if (match('-'))
                    {
                        double rhs = 0.0;
                        if (!parseTerm(rhs))
                            return false;
                        outValue -= rhs;
                        continue;
                    }

                    return true;
                }
            }

            bool parseTerm(double& outValue)
            {
                if (!parseFactor(outValue))
                    return false;

                while (true)
                {
                    skipWhitespace();
                    if (match('*'))
                    {
                        double rhs = 0.0;
                        if (!parseFactor(rhs))
                            return false;
                        outValue *= rhs;
                        continue;
                    }

                    if (match('/'))
                    {
                        double rhs = 0.0;
                        if (!parseFactor(rhs))
                            return false;
                        if (std::abs(rhs) <= 0.000000000001)
                        {
                            errorText = "division by zero";
                            return false;
                        }
                        outValue /= rhs;
                        continue;
                    }

                    return true;
                }
            }

            bool parseFactor(double& outValue)
            {
                skipWhitespace();

                if (match('+'))
                    return parseFactor(outValue);

                if (match('-'))
                {
                    if (!parseFactor(outValue))
                        return false;
                    outValue = -outValue;
                    return true;
                }

                if (match('('))
                {
                    if (!parseExpression(outValue))
                        return false;
                    skipWhitespace();
                    if (!match(')'))
                    {
                        errorText = "')' expected";
                        return false;
                    }
                    return true;
                }

                const auto ch = currentChar();
                if (isNumberStart(ch))
                    return parseNumber(outValue);

                if (isIdentifierStart(ch))
                {
                    juce::String identifier;
                    if (!parseIdentifier(identifier))
                        return false;
                    return parseIdentifierValue(identifier, outValue);
                }

                errorText = "unexpected token near '" + juce::String::charToString(ch) + "'";
                return false;
            }

            bool parseNumber(double& outValue)
            {
                const auto remaining = expression.substring(position).trimStart();
                const auto remainingStd = remaining.toStdString();
                if (remainingStd.empty())
                {
                    errorText = "number expected";
                    return false;
                }

                char* endPtr = nullptr;
                const auto parsed = std::strtod(remainingStd.c_str(), &endPtr);
                if (endPtr == remainingStd.c_str())
                {
                    errorText = "number expected";
                    return false;
                }
                if (!std::isfinite(parsed))
                {
                    errorText = "number is not finite";
                    return false;
                }

                const auto consumed = static_cast<int>(endPtr - remainingStd.c_str());
                // Re-scan from the original cursor so position tracks untrimmed spacing.
                skipWhitespace();
                position += consumed;
                outValue = parsed;
                return true;
            }

            bool parseIdentifier(juce::String& outIdentifier)
            {
                skipWhitespace();
                if (!isIdentifierStart(currentChar()))
                {
                    errorText = "identifier expected";
                    return false;
                }

                const auto start = position;
                ++position;
                while (!isAtEnd() && isIdentifierBody(currentChar()))
                    ++position;

                outIdentifier = expression.substring(start, position).trim();
                if (outIdentifier.isEmpty())
                {
                    errorText = "identifier expected";
                    return false;
                }

                return true;
            }

            bool parseIdentifierValue(const juce::String& identifier, double& outValue)
            {
                auto toNumericValue = [this](const juce::var& value, const juce::String& key, double& converted) -> bool
                {
                    if (value.isInt() || value.isInt64() || value.isDouble())
                    {
                        converted = static_cast<double>(value);
                        if (!std::isfinite(converted))
                        {
                            errorText = "param '" + key + "' is not finite";
                            return false;
                        }
                        return true;
                    }

                    if (value.isBool())
                    {
                        converted = static_cast<bool>(value) ? 1.0 : 0.0;
                        return true;
                    }

                    if (value.isString())
                    {
                        const auto text = value.toString().trim();
                        if (text.isEmpty())
                        {
                            errorText = "param '" + key + "' cannot be converted to number";
                            return false;
                        }

                        const auto textStd = text.toStdString();
                        char* endPtr = nullptr;
                        const auto parsed = std::strtod(textStd.c_str(), &endPtr);
                        if (endPtr == textStd.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
                        {
                            errorText = "param '" + key + "' cannot be converted to number";
                            return false;
                        }

                        converted = parsed;
                        return true;
                    }

                    errorText = "param '" + key + "' has unsupported type";
                    return false;
                };

                if (const auto it = runtimeParams.find(identifier); it != runtimeParams.end())
                    return toNumericValue(it->second, identifier, outValue);

                for (const auto& entry : runtimeParams)
                {
                    if (!entry.first.equalsIgnoreCase(identifier))
                        continue;
                    return toNumericValue(entry.second, entry.first, outValue);
                }

                errorText = "unknown runtime param '" + identifier + "'";
                return false;
            }

            void skipWhitespace()
            {
                while (!isAtEnd() && juce::CharacterFunctions::isWhitespace(currentChar()))
                    ++position;
            }

            bool isAtEnd() const noexcept
            {
                return position >= expression.length();
            }

            juce::juce_wchar currentChar() const noexcept
            {
                if (isAtEnd())
                    return 0;
                return expression[position];
            }

            bool match(juce::juce_wchar expected)
            {
                if (currentChar() != expected)
                    return false;
                ++position;
                return true;
            }

            static bool isAsciiDigit(juce::juce_wchar ch) noexcept
            {
                return ch >= '0' && ch <= '9';
            }

            static bool isIdentifierStart(juce::juce_wchar ch) noexcept
            {
                return (ch >= 'a' && ch <= 'z')
                    || (ch >= 'A' && ch <= 'Z')
                    || ch == '_';
            }

            static bool isIdentifierBody(juce::juce_wchar ch) noexcept
            {
                return isIdentifierStart(ch) || isAsciiDigit(ch) || ch == '.';
            }

            static bool isNumberStart(juce::juce_wchar ch) noexcept
            {
                return isAsciiDigit(ch) || ch == '.';
            }
        };
    };
}
