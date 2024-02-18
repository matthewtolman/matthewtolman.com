
Often I hear developer journeys through the programming languages they used or the companies they worked for. Sometimes it's told through frameworks learned or projects released or mistakes made. I'll probably tell my story a few times using some of those common methods. But today I thought I'd do something different. 

[toc]

## First Steps

My very first editor was the one that came bundled with QBasic back when I was a kid. I have no idea what it was called, or if it had a different name at all. All I remmember was white text on a blue background. It used keyboard shortcuts for the menu items, and I was very confused as to why I couldn't just use a mouse to do things. I used that for a few years doing QBasic, and then I started to get bored of programming. Until I joined a robotics club at school and started to code in some sort of C variant.

The variant of C that I used was adapted for kids and didn't do any memory allocation (or use pointers). Everything was passed by value, and it was very easy to use. We mostly just called methods like `move_forward`. The software for the club came bundled with an editor. By modern or contemporary standards, this editor would be considered barebones. But for a kid who only had used QBasic, it was a technological marvel. The biggest game changer was that I could use a mouse. I could click on buttons, I could select text, I could resize the window. It was awesome. After that, I decided I wanted to learn C++, so I asked my dad to teach me (he's a game programmer and knew C++ really well).

My next two editors were Notepad++, and an old copy of Visual Studio that my dad had a license he wasn't using. I mostly used Notepad++ for HTML and XML files, and I used Visual Studio for everything else. I had no idea what a build system did or what a Visual Studio project was. Whenever I messed up my project, I had to ask my dad for help. He would then open up the project files in Notepad++ and start editing them manually. I picked up a few things from watching him, and I was able to start fixing some of my configuration issues.

In high school I started working on websites for my neighbor. I got paid a little bit, but not much. With this job, I was working with ASP (not ASP.NET) and MySQL. Visual Studio could do ASP, and I started to use MySQL Workbench for the database.

Meanwhile, I continued doing robotics clubs throughout High School. There I was using Eclipse and Netbeans (at some point the editor was changed, though I don't remember why or when). At some point during all of this, I realized that Visual Studio couldn't do everything. My conclusion was that I just needed different editors depending on the programming language I was using. Eclipse for Java, Visual Studio for ASP and C++, Netbrains for websites (and sometimes C++), and Notepad++ as a fallback for everything else. 

## College Years

While I was in college, Microsoft dramatically changed the editor landscape. First, they made a free tier of Visual Studio. This meant that I could start using a modern version of the editor instead of my dad's old copy (which was over a decade old at this point). Second, Microsoft released Visual Studio Code. VSCode blew my mind with how good it was and how fast it was. Especially when it came to JavaScript support.

At the time, I was also working for a startup. For the first bit, I was still using Netbeans, though it eventually changed to VSCode. As part of my job, I also had to access and edit some files over SSH, so I got a basic understanding of some Vim commands. However, I only thought of Vim as "notepad over SSH" and I didn't realize that Vim plugins existed.

Some of my college classes also started using some cloud-based editors. Which were all just VSCode running in a virtual machine that you then accessed in a browser. For a classroom it was nice since the instructor could create a template machine that we could all fork. This meant the instructor and TAs didn't have to deal with questions on how to get a compiler working for the first few weeks of class. Unfortunately, the service we used was bought by a much bigger company, and then stripped for parts and shutdown. 

## Emacs Away

After college, I left the startup and started work at a bigger company. There were quite a few meetup groups inside the company that I joined, one of which was for Clojure. It was here that I learned that terminal based editors (like Vim and Emacs) could actually be serious development environments. Though the Clojurists really liked the Vim vs Emacs debate. Fortunately the lead of the group knew that only supporting Vim or Emacs would stunt the group's ability to grow. So he recommended that all newcomers to the language get started with IntelliJ and a paid plugin called Cursive. He also always used this combination when presenting to the group, that way newcomers could see how to use the language in a familiar editor. When I first started learning Clojure, I started using IntelliJ + Cursive.

Once I was familiar with Clojure, I became curious about these terminal editors. One of my coworkers was also getting curious about them. He and I had learned Clojure together and were writing a Clojure service together. So we thought we'd learn how to use terminal editors together as well. We didn't know which one was better, or really what the difference was. And the Clojurists were so biased and overly opinionated that asking felt like a fool's errand. So we decided one of us would learn Vim and the other Emacs. I had never tried Emacs, so that's what I picked.

We then spent several months trying out our editors. We each found a local expert for our editor of choice who helped us get things setup. We then used our editors a lot, and I was amazed by how powerful terminal editors could be. And yes, I did use Emacs in the terminal. I was trying to learn a terminal editor, not a GUI with keyboard shortcuts. At one point, I got frustrated that the emacs command launched a GUI window by default. So I went into my system config and remapped the command to only launch the terminal version.

Over the next few months, my coworker and I talked about the two editors and our experiences. As far as coding features and functionality went, the two editors were about the same. There were similar plugins that provided the same functionality. There were shortcuts which did the same thing. We didn't find any noteable features lacking in either editor.

Non-coding functionality was better in Emacs - but only if you used the GUI version. In the GUI version, you could view images, play Tetris, browse the internet, etc. In the terminal version, a lot of that broke, to the point that Vim was arguably better.

Ergonomically, Vim was much better. It wasn't even close. And ergonomics was becoming important for me. My wrists and fingers hurt when I used Emacs. I had already been using an ergonomic keyboard, but Emacs undid any benefit I got and then made it worse. It got bad enough that I had to stop using Emacs and go back to IntelliJ. At one point I tried using Spacemacs to have Vim keybindings, but a lot of the Spacemacs plugins were broken or undocumented. Plus, using Spacemacs meant that I had to relearn all of my keybindings.

I was pretty heartbroken. I had spent months learning how to use my editor, and I could no longer use it. Even though Vim promised to be better, I wasn't in a spot to spend an additional several months learning how to configure and use yet another editor on just the promise of less pain. So I went back to JetBrains, and my pain got a lot better. I also started using a lot of the JetBrains product suite, such as CLion, PHPStorm, and DataGrip.

## Jetting to mediocrity

Since then, I've primarily used JetBrains products. A few times Visual Studio would add a new feature that I really wanted (e.g. remote debugging) so I'd switch to that for Windows. And then JetBrains would add the same feature, so I'd switch back. But I didn't really use many other editors during this time.

JetBrains products come "ready out of the box." For a long time, the only customization I would do was bind the "Clone Caret Above/Below" actions. No additional plugins, no other settings changes. It was a quick setup process, and I had the same setup process regardless of the operating system being used. For me, this is what I needed after months of using Emacs. I wasn't ready to spend a lot of time configuring another editor. Or any other editor. I had stopped using VSCode too during this time since it required configuration and setup for my use cases. And even just the idea of installing plugins on every operating system was a little much (I regularly program on Windows, Mac, and Linux).

Eventually, I started installing JetBrains plugins. Mostly it was plugins that I'd get auto prompted to install, such as support for a new file type. Eventually I started searching out plugins that brought back some of the features I had missed from my Emacs config, like Rainbow Brackets and AceJump. For a long time, JetBrains editors were heavy, but they weren't unbearable.

But, as the years came so did the updates. And with the updates, came the bloat. The amount of times I had to increase the editor memory grew and grew. Nowadays, I just change the memory limit to be just over 4GB the moment I install the editor. It now takes up more memory than my high school computer had.

And it became more unstable. My CLion intellisense process now regularly crashes, and in order to get any autocomplete I have to restart the editor. This happens on every operating system, and is really annoying. My PHPStorm's debugger also corrupts itself and stops connecting after every MacOS update as well. The only workaround I have is to reinstall PHP and then remove my `.idea` folder in my project, followed by reconfiguring all of my project settings. I haven't had an update not break PHPStorm for 4 years now. I've also had moments where my DataGrip forgets my server credentials after an update. And I've had constant issues with IntelliJ's Maven integration, to the point that I no longer use IntelliJ's Maven integration and just run all of my Maven commands through the terminal.

JetBrains has gone from "excellent" to either "good" or "meh" depending on the product. I will still use it for now since my workflows are currently based around IntelliJ, and it's the best cross-platform low-configuration editor I have. However, I'm now asking the question "what comes next?"

I've taken a look at several new editors out there. Most of them are language exclusive (e.g. Rust or Clojure only), or they're platform exclusive (usually to MacOS), or they're incomplete. Some of them I'm really hopeful for (like zed and Onivim 2). However, I'm looking for a new editor now, so I need to look at editors that are available and which meet my needs of a cross-platform experience. Plus I'd like something that's well-supported and isn't going to suddenly shutter its doors. So instead of looking at new editors, I started looking at old ones.

## Restarting with Vim

I decided I should take another whack at terminal based editors. This time, I'm not going with Emacs. The ergonomics are really too terrible. Instead, I'm going Vim, and more specifically I'll be using NeoVim.

My first step was to get a base Neovim configuration that I could easily change. I didn't want to start purely from scratch, but I also didn't want too much bloat. One of the things I learned from Emacs is that it's really easy to learn and understand plugins as they're added in gradually, but it's almost impossible to figure out what's going on when they're added all at once (usually by using someone else's setup). So I decided I'd start with [kickstart.nvim](https://github.com/nvim-lua/kickstart.nvim).

I pretty quickly ran into an issue. I was on my Mac with the built-in Mac terminal. And Vim was really hard to read. All the colors were too low contrast with the background, and no amount of chaning the terminal or Vim theme fixed it. I had never had an issue with the Linux terminal, and I didn't have an issue in the new Windows Terminal app. It was just Mac. So, I decided I'd install a new terminal. I went with Alacritty since I had heard of them years before, I had tried them a few times, and they were still around. And as soon as I switched to Alacritty, my color issues went away.

Next, I started adding my plugins, seeing if I liked them, and choosing whether to keep them or get rid of them. I got several language servers installed and setup. Now with my plugins and my terminal issues fixed, I started coded.

And then I realized my coding workflow didn't work.

I was used to CLion where the IDE automatically ran CMake and extracted the compiler commands automatically. CLion also extracted the commands for every build that I did. And some of my projects had a lot of builds. I could have 4-12 different builds for a single operating system, and I was building for multiple operating systems. These builds included different targets, such as native, WASM, and Cheerp (C++ to JavaScript). They would also include debug and release, coverage builds, sanitization builds, and builds with or without custom developer tooling.

Plus, I heavily used CLion's remote debugging features so that I could build and test on multiple operating systems. I would use both physical machines (e.g. Mac) and virtual machines through WSL (Debian, Arch, Alpine, etc). This was a ton of build information that CLion was managing for me automatically. And all I had to do in CLion was change a dropdown selection and then all of my intellisense would adapt to the platform and build that I was using. This made it really easy to quickly test multiple targets with different build settings.

However, Vim doesn't automatically extract build options and then conveniently switch between them. Instead, the process is more manual. Which means that for my intellisense to work with Vim I have to do more manual work.

It also meant that my remote debugging doesn't exist anymore. I only know of two editors with remote C++ debugging, and that's CLion and Visual Studio. And Visual Studio only supports remote debugging C++ code on Linux. Meanwhile, I'm using remote debugging for both Linux and Mac.

This meant that I either had to rebuild my workflow, change my workflow, or be stuck with CLion until someone else creates an editor that happens to fit my workflow.

So, I started taking a look into my workflow. I found a lot of occurrences of the XY problem. For instance, with remote debugging it was because I didn't have an easy way to plug and unplug my devices and synchronize the files. A KVM switch combined with rsync solves the issue.

With the build management issue, this can be solved with a script. All of my build presets are stored in a JSON file for each project. A script can simply load that file and then manage switching between the builds for me. I can place this script on the path and then run it from any project.

The other roadblock is learning Vim shortcuts. For this, I'm going to install IdeaVim and practice the shortcuts while building out my new workflow. In the end, I'll end up both knowing Vim and also having an editor-agnostic workflow for cross-platform development. That way if I ever change editors again I don't have to rebuild my workflow.

## Wrapping up

I've journeyed through quite a few editors over the years. Some of my time with editors have been short-lived, while others lasted for a long time. I've gone back and forth a few times, but I found a local maximum in my editor journey.

But now I'm ready to continue my journey. It's been a good resting place, and I've been able to focus on developing other skills while I was here. But the signs are here that it's time to travel on, so I'm going to continue my journey. Maybe at some point I'll tell the next chapter. Or maybe I'll tell my tale in another way. I'll see where the road leads.
