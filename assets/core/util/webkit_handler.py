# This module is intended to keep track on the webkit hooks that are added to the browser
import json
import os
import pprint
import sys
import traceback
import Millennium # type: ignore
from util.logger import logger

class WebkitHookStore:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance.stack = []
        return cls._instance

    def push(self, item):
        self.stack.append(item)

    def unregister_all(self):
        for hook in self.stack.copy():
            Millennium.remove_browser_module(hook)
            self.stack.remove(hook)


def parse_conditional_patches(conditional_patches: dict, theme_name: str):
    webkit_items = []

    with open(os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), "themes.json")) as file:
        theme_conditions = json.load(file).get("conditions", {})

        # Add condition keys into the webkit_items array
        for item, condition in conditional_patches.get('Conditions', {}).items():
            for value, control_flow in condition.get('values', {}).get(theme_conditions[theme_name][item], {}).items():
                if isinstance(control_flow, dict): 
                    affects = control_flow.get('affects', [])
                    if isinstance(affects, list): 
                        for match_string in affects:
                            target_path = control_flow.get('src')

                            webkit_items.append({
                                'matchString': match_string,
                                'targetPath': target_path,
                                'fileType': value
                            })
                

        patches = conditional_patches.get('Patches', [])
        for patch in patches:  
            for inject_type in ['TargetCss', 'TargetJs']:

                if inject_type in patch:
                    target_css = patch[inject_type]
                    if isinstance(target_css, str):
                        target_css = [target_css]

                    for target in target_css:
                        webkit_items.append({
                            'matchString': patch['MatchRegexString'],
                            'targetPath': target,
                            'fileType': inject_type
                        })

        # Remove duplicates
        seen = set()
        unique_webkit_items = []
        for item in webkit_items:
            identifier = (item['matchString'], item['targetPath'])
            if identifier not in seen:
                seen.add(identifier)
                unique_webkit_items.append(item)

        logger.log(str(unique_webkit_items))
        return unique_webkit_items

conditional_patches = []

def add_browser_css(css_path: str, regex=".*") -> None:
    stack = WebkitHookStore()
    stack.push(Millennium.add_browser_css(css_path, regex))

def add_browser_js(js_path: str, regex=".*") -> None:
    stack = WebkitHookStore()
    stack.push(Millennium.add_browser_js(js_path, regex))

def remove_all_patches() -> None:
    index = 0
    while index < len(conditional_patches):
        Millennium.remove_browser_module(conditional_patches[index][1])
        del conditional_patches[index] 

def add_conditional_data(path: str, data: dict, theme_name: str):
    try:
        remove_all_patches()
        parsed_patches = parse_conditional_patches(data, theme_name)

        for patch in parsed_patches:
            if patch['fileType'] == 'TargetCss' and patch['targetPath'] is not None and patch['matchString'] is not None:
                target_path = os.path.join(path, patch['targetPath'])
                conditional_patches.append((target_path, Millennium.add_browser_css(target_path, patch['matchString'])))

            elif patch['fileType'] == 'TargetJs' and patch['targetPath'] is not None and patch['matchString'] is not None:
                target_path = os.path.join(path, patch['targetPath'])
                conditional_patches.append((target_path, Millennium.add_browser_js(target_path, patch['matchString'])))

    except Exception as e:
        logger.log(f"Error adding conditional data: {e}")
        # log error to stderr
        sys.stderr.write(traceback.format_exc())