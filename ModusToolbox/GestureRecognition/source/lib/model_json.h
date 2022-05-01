#ifndef __MODEL_JSON_H__
#define __MODEL_JSON_H__

const char recognition_model_string_json[] = {"{\"NumModels\":1,\"ModelIndexes\":{\"0\":\"mypipeline_rank_4\"},\"ModelDescriptions\":[{\"Name\":\"mypipeline_rank_4\",\"ClassMaps\":{\"1\":\"Horizontal\",\"2\":\"Stationary\",\"3\":\"Vertical\",\"0\":\"Unknown\"},\"ModelType\":\"DecisionTreeEnsemble\",\"FeatureFunctions\":[\"AbsoluteAreaofLowFrequency\",\"ThresholdCrossingRate\",\"PowerSpectrum\"]}]}"};

int recognition_model_string_json_len = sizeof(recognition_model_string_json);

#endif /* __MODEL_JSON_H__ */
