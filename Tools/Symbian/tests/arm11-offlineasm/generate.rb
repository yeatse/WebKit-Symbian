#!/usr/bin/env ruby
# The fixture has no C++ layout dependencies. Produce the normal extractor
# envelope for its selected configuration, then invoke the real asm.rb CLI.
require 'fileutils'
require 'rbconfig'
require 'set'
root, fixture, output = ARGV
abort 'usage: generate.rb checkout fixture.asm output-directory' unless output
backend = File.join(root, 'Source/JavaScriptCore/offlineasm')
$LOAD_PATH.unshift(backend)
require 'backends'
require 'offsets'
require 'parser'
require 'settings'
require 'transform'
ast = parse(fixture, {}, Set.new)
combinations = computeSettingsCombinations(ast)
index = combinations.index { |settings| settings['ARM'] }
abort 'ARM backend missing' unless index
low = ast.resolveSettings(combinations[index]).demacroify({})
abort 'fixture must not require synthetic C++ offsets' unless (offsetsList(low) + sizesList(low) + constsList(low)).empty?
FileUtils.mkdir_p(output)
offsets = File.join(output, 'probe.offsets')
File.binwrite(offsets, (OFFSET_HEADER_MAGIC_NUMBERS + OFFSET_MAGIC_NUMBERS + [index]).pack('q<*'))
header = File.join(output, 'ProbeAssembly.h')
abort 'offlineasm generation failed' unless system(RbConfig.ruby, File.join(backend, 'asm.rb'), fixture, offsets, header, 'normal')
# The CPU execution check below also rejects unsupported instructions. This
# guard makes accidental selection of the Thumb-2 backend easy to diagnose.
abort 'non-ARMv6 instruction in generated code' if File.read(header).match?(/"(?:movw|movt|rbit|dmb|it\s|\.thumb)/)
