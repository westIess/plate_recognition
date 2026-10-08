"""Integration tests for metrics, missing rows, quoting, and malformed inputs."""
import csv
import pathlib
import subprocess
import sys
import tempfile

exe = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as d:
    root = pathlib.Path(d)
    def write(name, rows):
        p = root / name
        with p.open('w', encoding='utf-8-sig', newline='') as f:
            csv.writer(f).writerows(rows)
        return p
    header = ['filename', 'plate_number']
    truth = write('truth.csv', [header, ['a,"x".jpg', 'A123BC77'], ['b.jpg', 'O482MP199']])
    results = write('results.csv', [header, ['a,"x".jpg', 'A125BC77']])
    errors = root / 'errors.csv'
    def run(expected):
        p = subprocess.run([exe, str(results), str(truth), str(errors)], capture_output=True, text=True)
        assert p.returncode == expected, p.stdout + p.stderr
        return p.stdout
    output = run(2)
    assert 'exact=0\n' in output and 'edit_distance=10\n' in output and 'reference_characters=17\n' in output, output
    assert 'CER=0.588235' in output and 'missing=1' in output, output
    with errors.open(newline='') as f:
        rows = list(csv.DictReader(f))
    assert rows[0]['filename'] == 'a,"x".jpg' and rows[1]['reason'] == 'missing_result'
    results = write('results.csv', [header, ['a,"x".jpg', 'A123BC77'], ['b.jpg', '0482MP199'], ['extra.jpg', '']])
    output = run(2)
    assert 'exact=1\n' in output and 'edit_distance=1\n' in output and 'extra=1' in output, output
    results = write('results.csv', [header, ['a,"x".jpg', 'A123BC77'], ['b.jpg', 'О482МР199']])
    output = run(0)
    assert 'accuracy=1.000000' in output and 'CER=0.000000' in output, output
    results = write('results.csv', [header, ['b.jpg', ''], ['b.jpg', '']])
    run(1)
    results.write_text('filename,plate_number\n"unterminated', encoding='utf-8')
    run(1)
    truth = write('truth.csv', [header, ['negative.png', '']])
    results = write('results.csv', [header])
    output = run(2)
    assert 'exact=0\n' in output and 'CER=N/A' in output
    results = write('results.csv', [header, ['negative.png', '']])
    assert 'accuracy=1.000000' in run(0)
print('evaluation integration tests passed')
