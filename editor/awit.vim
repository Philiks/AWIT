" Vim syntax file
" Language: AWIT
" Maintainer: Felix Janopol Jr.
" Latest Revision: 27 September 2021
" Filenames: *.awit
" Version: 0.1

" Usage:
" Put this file in .vim/syntax/awit.vim
" then add in your .vimrc file this line:
" autocmd BufRead,BufNewFile *.awit set filetype=awit

if exists("b:current_syntax")
  finish
endif

" String
syntax match awitString "\".*\"" contained

" Number 
syntax match awitNumber "-\?\d\+\.\?\d*" contained

" Assignment
syntax match awitAssignment "=" contained nextgroup=awitString skipwhite
syntax match awitAssignment "=" contained nextgroup=awitNumber skipwhite
"syntax match awitAssignment "=" contained nextgroup=awitLitKeywords skipwhite

" Identifier
syntax match awitIdentifier "\a\w*" contained nextgroup=awitAssignment skipwhite

" Todo
syntax keyword awitTodo contained TODO XXX TANDAANG GAWIN

" Language Keywords
syntax keyword awithOpKeywords contained at o
syntax keyword awithLitKeywords contained mali tama null
syntax keyword awithVarKeywords kilalanin nextgroup=awitIdentifier skipwhite
syntax keyword awitKeywords gawain nextgroup=awitIdentifier skipwhite
syntax keyword awitKeywords gawin habang ibalik
syntax keyword awitKeywords ipakita itigil ito ituloy kada
syntax keyword awitKeywords kundiman kung mula uri 

" Comment
syntax region awitComment start="//" end="$" contains=awitTodo

" Block
syntax region awitBlock start="{" end="}" fold transparent

" Set highlight
highlight default link awitString String 
highlight default link awitNumber Constant
highlight default link awitAssignment Statement
highlight default link awitIdentifier Identifier
highlight default link awitTodo Todo
highlight default link awitOpKeywords Operator
highlight default link awitLitKeywords Constant
highlight default link awitVarKeywords Statement
highlight default link awitKeywords Special
highlight default link awitComment Comment
highlight default link awitBlock Statement

let b:current_syntax = "AWIT"
