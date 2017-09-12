#!/usr/bin/env ruby
# Translate a string to "sendkey" commands for QEMU.
# Martin Vidner, MIT License

# https://en.wikibooks.org/wiki/QEMU/Monitor#sendkey_keys
# sendkey keys
#
# You can emulate keyboard events through sendkey command. The syntax is: sendkey keys. To get a list of keys, type sendkey [tab]. Examples:
#
#     sendkey a
#     sendkey shift-a
#     sendkey ctrl-u
#     sendkey ctrl-alt-f1
#
# As of QEMU 0.12.5 there are:
# shift     shift_r     alt     alt_r   altgr   altgr_r
# ctrl  ctrl_r  menu    esc     1   2
# 3     4   5   6   7   8
# 9     0   minus   equal   backspace   tab
# q     w   e   r   t   y
# u     i   o   p   ret     a
# s     d   f   g   h   j
# k     l   z   x   c   v
# b     n   m   comma   dot     slash
# asterisk  spc     caps_lock   f1  f2  f3
# f4    f5  f6  f7  f8  f9
# f10   num_lock    scroll_lock     kp_divide   kp_multiply     kp_subtract
# kp_add    kp_enter    kp_decimal  sysrq   kp_0    kp_1
# kp_2  kp_3    kp_4    kp_5    kp_6    kp_7
# kp_8  kp_9    <   f11     f12     print
# home  pgup    pgdn    end     left    up
# down  right   insert  delete

require "optparse"

# incomplete! only what I need now.
KEYS = {
  "%" => "shift-5",
  "/" => "slash",
  ":" => "shift-semicolon",
  "=" => "equal",
  "." => "dot",
  " " => "spc",
  "-" => "minus",
  "_" => "shift-minus",
  "*" => "asterisk",
  "," => "comma",
  "+" => "shift-equal",
  "|" => "shift-backslash",
  "\\" => "backslash",
}

class Main
  attr_accessor :command
  attr_accessor :delay_s
  attr_accessor :keystring

  def initialize
    self.command = nil
    self.delay_s = 0.1

    OptionParser.new do |opts|
      opts.banner = "Usage: sendkeys [-c command_to_pipe_to] STRING\n" +
        "Where STRING can be 'ls<enter>ls<gt>/dev/null<enter>'"

      opts.on("-c", "--command COMMAND",
              "Pipe sendkeys to this commands, individually") do |v|
        self.command = v
      end
      opts.on("-d", "--delay SECONDS", Float,
              "Delay SECONDS after each key (default: 0.1)") do |v|
        self.delay_s = v
      end
    end.parse!
    self.keystring = ARGV[0]
  end

  def sendkey(qemu_key_name)
    if qemu_key_name == "wait"
      sleep 1
    else
      if qemu_key_name =~ /[A-Za-z]/ && qemu_key_name == qemu_key_name.upcase
        key = "shift-#{qemu_key_name.downcase}"
      else
        key = qemu_key_name
      end
      qemu_cmd = "sendkey #{key}"
      if command
        system "echo '#{qemu_cmd}' | #{command}"
      else
        puts qemu_cmd
        $stdout.flush             # important when we are piped
      end
      sleep delay_s
    end
  end

  PATTERN = /
              \G  # where last match ended
              < [^>]+ >
            |
              \G
              .
            /x
  def run
    keystring.scan(PATTERN) do |match|
      if match[0] == "<"
        key_name = match.slice(1..-2)
        sendkey case key_name
                when "lt" then "shift-comma"
                when "gt" then "shift-dot"
                else key_name
                end
      else
        sendkey KEYS.fetch(match, match)
      end
    end
  end
end

Main.new.run
