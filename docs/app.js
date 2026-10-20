(function () {
"use strict";
const output=document.getElementById("terminal-output");
const form=document.getElementById("terminal-form");
const input=document.getElementById("terminal-input");
const clear=document.getElementById("clear-console");
const state=document.getElementById("console-state");
const commands={
help:()=>["Available demo commands:","  help       List available commands","  status     Show simulated system status","  version    Show project identity","  modules    List documented module groups","  memory     Show a simulated memory map","  clear      Clear the console output","  date       Show the browser's local date"],
status:()=>["SYSTEM STATUS / SIMULATED","Web UI .............. ONLINE","Native host .......... NOT CONNECTED","Kernel module ........ NOT LOADED BY THIS SITE","Authentication ....... DISABLED","Mode ................ DEMONSTRATION"],
version:()=>["AETHEROS NEXUS / OS LAB","Web console: 1.0.0","Native implementation: see repository main branch","Build target: C11 / GNU Make","The browser demo and native application are separate."],
modules:()=>["DOCUMENTED COMPONENT GROUPS","boot      Boot screen and initialization concepts","login     User/session scaffolding","screen    Terminal rendering helpers","menus     Text menus and dispatch","modules   Module registry scaffold","sound     Tone/effect demonstration","kernel    Separate Linux loadable-module experiment"],
memory:()=>["SIMULATED MEMORY MAP (illustrative only)","0x0000 - 0x3FFF  ROM / firmware concept","0x4000 - 0xBFFF  Application region concept","0xC000 - 0xFFFF  I/O and reserved concept","This is not a report of real or emulated RAM."],
date:()=>["Browser local time: "+new Date().toString()]
};
function appendLine(text,className){const p=document.createElement("p");if(className)p.className=className;p.textContent=text;output.appendChild(p)}
function run(raw){const command=raw.trim().toLowerCase();if(!command)return;appendLine("visitor@nexus:~$ "+raw,"muted");if(command==="clear"){output.replaceChildren();return}if(Object.prototype.hasOwnProperty.call(commands,command)){const result=commands[command]();result.forEach((line,index)=>appendLine(line,index===0&&command==="status"?"success":""))}else appendLine("Unknown demo command: "+command+" (type help)","error");output.scrollTop=output.scrollHeight}
form.addEventListener("submit",function(event){event.preventDefault();run(input.value);input.value="";state.textContent="DEMO";input.focus()});
clear.addEventListener("click",function(){output.replaceChildren();appendLine("Console cleared. Type help to get started.","muted");state.textContent="READY";input.focus()});
})();