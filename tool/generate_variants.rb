#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.dirname(__FILE__))
require "English"

module Unicode
  module_function
  def to_utf8(code_point)
    [code_point].pack("U")
  end

  def from_utf8(utf8)
    utf8.unpack("U")[0]
  end
end

class TwoCharTextParser
  def initialize
    @pages = {}
  end

  def parse(input)
    parse_two_char_text(input)
  end

  def sorted_pages
    @pages.sort_by do |page, characters|
      page
    end
  end

  private
  def parse_two_char_text(input)
    current_page = nil
    characters = nil
    input.each_line do |line|
      original_code = "0x%05x" % Unicode.from_utf8(line[0])
      normalized_code = "0x%05x" % Unicode.from_utf8(line[1])
      # original_code = "0x%05x" % Unicode.from_utf8(line[1])
      # normalized_code = "0x%05x" % Unicode.from_utf8(line[0])
      if original_code[2, 1] == "0"
        current_page = original_code[3, 2]
        if @pages[current_page].nil?
          @pages[current_page] = []
          puts characters
          puts current_page
          puts "#{line[0]}#{line[1]}"
          # puts "#{line[1]}#{line[0]}"
          characters = @pages[current_page]
        end
        characters << {:original_code => original_code, :normalized_code => normalized_code}
      else
        # puts "invalid code #{original_code}"
      end
    end
  end
end

if ARGV.size != 1
  puts("Usage: #{$0} two character file (1:original_character 2:normalized_character)")
  exit(false)
end

two_char_text_path = ARGV[0]

parser = TwoCharTextParser.new
File.open(two_char_text_path) do |two_char_text|
  parser.parse(two_char_text)
end

parser.sorted_pages.each do |page, characters|
  puts "static uint32_t unicode_ci_custom_page_#{page}[] = {"
  for i in 0..255
    code = "0x0#{page}%02x" % i
    characters.each do |character|
      if code == character[:original_code]
        code = character[:normalized_code]
        break
      end
    end

    if i == 0
      print "  "
    end
    print "#{code}"
    if i < 255
      print ", "
    end 
    if (i + 1) % 8 == 0
      print "\n"
      if i < 255
        print "  "
      end
    end 
  end
  puts "};"
  puts ""
end

puts "static uint32_t *unicode_ci_custom_table[256] = {"
for i in 0..255
  code = "%02x" % i
  exist_code = "                     NULL"
  parser.sorted_pages.each do |page, characters|
    if code == page
      exist_code = "unicode_ci_custom_page_#{page}"
      break
    end
  end
  if i == 0
    print "  "
  end
  print exist_code
  if i < 255
    print ", "
  end
  if (i + 1) % 2 == 0
    print "\n"
    if i < 255
      print "  "
    end
  end
end
puts "};"
