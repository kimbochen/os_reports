# OS Reports

## Description
This repo includes the reports on the 4 implementations of NachOS, an educational OS.

## Convert to PDF
* Pandoc is used to convert the markdown files into PDF files. \
  `header.yaml` is the configuration file.
* The command is as follows:
  `pandoc --pdf-engine=xelatex header.yaml MP<Number>_report_39.md -o MP<Number>_report_39.pdf`

