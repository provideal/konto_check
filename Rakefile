# Copyright (c) 2010 Provideal Systems GmbH
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

require 'rubygems'
require 'rake'
require 'rake/extensiontask'

Rake::ExtensionTask.new('konto_check')

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "konto_check"
    gem.summary = %Q{Checking german BICs/Bank account numbers}
    gem.description = %Q{Check whether a certain bic/account-no-combination can possibly be valid. It uses the C library kontocheck (see http://sourceforge.net/projects/kontocheck/) by Michael Plugge.}
    gem.email = "info@provideal.net"
    gem.homepage = "http://github.com/provideal/konto_check"
    gem.authors = ["Provideal Systems GmbH"]
    gem.add_development_dependency "thoughtbot-shoulda", ">= 0"
    #gem.files.exclude "ext"
    # gem is a Gem::Specification... see http://www.rubygems.org/read/chapter/20 for additional settings
  end
  Jeweler::GemcutterTasks.new
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: gem install jeweler"
end

require 'rake/testtask'
Rake::TestTask.new(:test) do |test|
  test.libs << 'lib' << 'test'
  test.pattern = 'test/**/test_*.rb'
  test.verbose = true
end

begin
  require 'rcov/rcovtask'
  Rcov::RcovTask.new do |test|
    test.libs << 'test'
    test.pattern = 'test/**/test_*.rb'
    test.verbose = true
  end
rescue LoadError
  task :rcov do
    abort "RCov is not available. In order to run rcov, you must: sudo gem install spicycode-rcov"
  end
end

task :test => :check_dependencies

task :default => :test

require 'rake/rdoctask'
Rake::RDocTask.new do |rdoc|
  version = File.exist?('VERSION') ? File.read('VERSION') : ""

  rdoc.rdoc_dir = 'rdoc'
  rdoc.title = "konto_check #{version}"
  rdoc.rdoc_files.include('README*')
  rdoc.rdoc_files.include('lib/**/*.rb')
end
