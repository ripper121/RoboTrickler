String jsonEscape(const String &input)
{
  String output;
  output.reserve(input.length() + 4);
  for (uint16_t i = 0; i < input.length(); i++)
  {
    char c = input.charAt(i);
    if ((c == '"') || (c == '\\'))
    {
      output += '\\';
      output += c;
    }
    else if (c == '\n')
    {
      output += "\\n";
    }
    else if (c == '\r')
    {
      output += "\\r";
    }
    else if (c == '\t')
    {
      output += "\\t";
    }
    else if ((uint8_t)c < 0x20)
    {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned char)c);
      output += escaped;
    }
    else
    {
      output += c;
    }
  }
  return output;
}
