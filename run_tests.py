"""
This Script was created by Gil Caspi & Tamar Behar

1. Make sure this script is located in your project root folder
    The tests should be under the root folder in a specific folder (for example: "input_examples")

2. Run this script from your virtual machine terminal (working dir = project root folder):
    "python3 run_tests.py --app ./bp_main --folder input_examples"

Note: This script will create a results folder inside the tests folder with each test output.
In addition, it will create a diff folder with the difference between the desired output
and the output received by your application
"""
import os
from glob import glob
import argparse


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Run tests to check your application')
    parser.add_argument("--app", type=str, default='./bp_main', required=False, help='The application path')
    parser.add_argument("--folder", type=str, default='benchmark50', required=False,
                        help='The tests folder (including both inputs and ground truth outputs)')
    args = parser.parse_args()
    app_path = args.app
    tests_folder = args.folder
    input_test_paths = glob(os.path.join(tests_folder, "*.trc"))
    results_folder = os.path.join(tests_folder, 'results')
    os.makedirs(results_folder, exist_ok=True)
    diff_folder_path = os.path.join(results_folder, 'diff')
    os.makedirs(diff_folder_path, exist_ok=True)

    for current_test_path in input_test_paths:
        test_name = os.path.split(current_test_path)[-1].split('.')[0]
        results_path = os.path.join(results_folder, test_name + '.out')
        os.system('{app_path} {input_test_path} > {results_output_path}'.format(app_path=app_path,
                                                                                input_test_path=current_test_path,
                                                                                results_output_path=results_path))

    for current_test_path in input_test_paths:
        test_name = os.path.split(current_test_path)[-1].split('.')[0]
        results_path = os.path.join(results_folder, test_name + '.out')
        input_test_gt_path = os.path.join(tests_folder, test_name + '.out')

        diff_path = os.path.join(diff_folder_path, 'diff_{}.txt'.format(test_name))
        os.system('diff {gt_path} {results_path} > {diff_path}'.format(gt_path=input_test_gt_path,
                                                                       results_path=results_path,
                                                                       diff_path=diff_path))

    passed_counter = 0
    total_counter = 0
    for i, current_test_path in enumerate(input_test_paths):
        total_counter += 1
        test_name = os.path.split(current_test_path)[-1].split('.')[0]
        diff_path = os.path.join(diff_folder_path, 'diff_{}.txt'.format(test_name))
        if os.path.getsize(diff_path) > 0:
            test_status = "FAILED"
        else:
            test_status = "PASSED"
            passed_counter += 1
        print("{} - {}".format(test_name, test_status))

    print("Passed {} out of {} tests".format(passed_counter, total_counter))
