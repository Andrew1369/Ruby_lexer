# classes & methods
class Greeter
  VERSION = "1.0.0"
  @@count = 0
  def initialize(name)
    @name = name
    @@count += 1
  end
  def greet(times = 1)
    times.times do |i|
      puts "Hello, #{@name}! ##{i+1}"
    end
  end
end
g = Greeter.new("Ruby")
g.greet(2)
